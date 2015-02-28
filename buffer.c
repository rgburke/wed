/*
 * Copyright (C) 2014 Richard Burke
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "wed.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "status.h"
#include "display.h"
#include "file.h"
#include "config.h"
#include "encoding.h"

static int resize_line_text_if_req(Line *, size_t);
static int resize_line_text(Line *, size_t);
static Status reset_buffer(Buffer *);
static int add_to_buffer(Buffer *, BufferPos *, const char *, size_t, int);
static int is_selection(Direction *);
static void default_movement_selection_handler(Buffer *, int, Direction *);
static Status pos_change_real_line(Buffer *, BufferPos *, Direction, int);
static Status pos_change_screen_line(Buffer *, BufferPos *, Direction, int);
static Status advance_pos_to_line_offset(Buffer *, BufferPos *, int);
static void update_line_col_offset(Buffer *, BufferPos *);
static BufferPos to_line_start(BufferPos);
static Status delete_line_segment(Buffer *, BufferPos, BufferPos);

Buffer *new_buffer(FileInfo file_info)
{
    Buffer *buffer = malloc(sizeof(Buffer));
    RETURN_IF_NULL(buffer);

    memset(buffer, 0, sizeof(Buffer));

    if ((buffer->config = new_hashmap()) == NULL) {
        free(buffer);
        return NULL;
    }

    buffer->file_info = file_info;
    init_bufferpos(&buffer->pos);
    init_bufferpos(&buffer->screen_start);
    init_bufferpos(&buffer->select_start);
    buffer->encoding_type = ENC_UTF8;
    init_char_enc_funcs(buffer->encoding_type, &buffer->cef);
    init_window_info(&buffer->win_info);

    return buffer;
}

Buffer *new_empty_buffer(const char *file_name)
{
    FileInfo file_info;

    if (!init_empty_fileinfo(&file_info, file_name)) {
        return NULL;
    }

    Buffer *buffer = new_buffer(file_info); 
    RETURN_IF_NULL(buffer);
    buffer->lines = buffer->pos.line = buffer->screen_start.line = new_line();

    if (buffer->lines == NULL) {
        free(buffer);
        return NULL;
    }

    buffer->line_num = buffer->byte_num = 1;

    return buffer;
}

void free_buffer(Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    free_fileinfo(buffer->file_info);
    free_config(buffer->config);

    Line *line = buffer->lines;
    Line *tmp = NULL;

    while (line != NULL) {
        tmp = line->next;
        free_line(line);
        line = tmp;
    }

    free(buffer);
}

Line *new_line(void)
{
    return new_sized_line(0);
}

Line *new_sized_line(size_t length)
{
    size_t alloc_num = (length / LINE_ALLOC) + 1;

    Line *line = malloc(sizeof(Line));
    RETURN_IF_NULL(line);

    if ((line->text = malloc(alloc_num * LINE_ALLOC)) == NULL) {
        free(line);
        return NULL;
    }

    line->alloc_num = alloc_num;
    line->length = 0; 
    line->screen_length = 0;
    *line->text = '\0';
    line->next = NULL;
    line->prev = NULL;

    return line;
}

void free_line(Line *line)
{
    if (line == NULL) {
        return;
    }

    free(line->text);
    free(line);
}

int init_bufferpos(BufferPos *pos)
{
    if (pos == NULL) {
        return 0;
    }

    pos->line = NULL;
    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;

    return 1;
}

TextSelection *new_textselection(Buffer *buffer, Range range)
{
    TextSelection *text_selection = malloc(sizeof(TextSelection));
    RETURN_IF_NULL(text_selection);

    if (range.start.line == range.end.line) {
        text_selection->type = TST_STRING;
        text_selection->text.string = get_line_segment(range.start.line, range.start.offset, range.end.offset);

        if (text_selection->text.string == NULL) {
            goto cleanup;
        }
    } else {
        text_selection->type = TST_LINE;
        BufferPos line_end = range.start;
        line_end.offset = line_end.line->length;
        Line *line = text_selection->text.lines = clone_line_segment(buffer, range.start, line_end);
        
        if (line == NULL) {
            goto cleanup;
        }

        line->prev = NULL;
        Line *prev = line;

        while ((line = line->next) != range.end.line) {
            line = clone_line(line);    

            if (line == NULL) {
                goto cleanup;
            }

            prev->next = line;
            line->prev = prev;
            prev = line;
        }

        BufferPos line_start = to_line_start(range.end);
        line = clone_line_segment(buffer, line_start, range.end); 

        if (line == NULL) {
            goto cleanup;
        }

        prev->next = line;
        line->prev = prev;
        line->next = NULL;
    }

    return text_selection;

cleanup:
    RETURN_IF_NULL(text_selection);

    if (text_selection->type == TST_STRING) {
        free(text_selection->text.string);
    } else if (text_selection->type == TST_LINE) {
        Line *line = text_selection->text.lines;
        Line *next;

        while (line != NULL) {
            next = line->next;
            free_line(line);
            line = next;
        }
    }

    free(text_selection);

    return NULL;
}

void free_textselection(TextSelection *text_selection)
{
    if (text_selection == NULL) {
        return;
    }

    if (text_selection->type == TST_STRING) {
        free(text_selection->text.string); 
    } else {
        Line *line = text_selection->text.lines;
        Line *next;

        while (line != NULL) {
            next = line->next;
            free_line(line);
            line = next;
        }
    }

    free(text_selection);
}

/* Returns deep copy of a line */
Line *clone_line(Line *line)
{
    Line *clone = malloc(sizeof(Line));
    RETURN_IF_NULL(clone);
    *clone = *line;
    clone->text = malloc(line->alloc_num * LINE_ALLOC);

    if (clone->text == NULL) {
        free(clone);
        return NULL;
    }

    memcpy(clone->text, line->text, line->length);

    return clone;
}

/* TODO When editing functionality is added this function
 * will need to expand and shrink a line.
 * Also need to consider adding a function to determine
 * how much to expand or shrink by.
 * Also pick better function name. */
static int resize_line_text_if_req(Line *line, size_t new_size)
{
    size_t allocated = line->alloc_num * LINE_ALLOC;
    int success = 1;

    if (new_size > allocated) {
        success = resize_line_text(line, new_size);  
    } else if (new_size < (allocated - LINE_ALLOC)) {
        success = resize_line_text(line, new_size);
    }

    return success;
}

static int resize_line_text(Line *line, size_t new_size)
{
    size_t alloc_num = (new_size / LINE_ALLOC) + 1;
    void *ptr = realloc(line->text, alloc_num * LINE_ALLOC);

    if (ptr == NULL) {
        return 0;
    }

    line->alloc_num = alloc_num;
    line->text = ptr;

    return 1;
}

Status clear_buffer(Buffer *buffer)
{
    Line *line = buffer->lines;
    Line *next;

    RETURN_IF_FAIL(reset_buffer(buffer));

    while (line != NULL) {
        next = line->next;
        free_line(line);
        line = next;
    }

    return STATUS_SUCCESS;
}

static Status reset_buffer(Buffer *buffer)
{
    buffer->lines = new_line();

    if (buffer->lines == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to reset buffer");
    }

    init_bufferpos(&buffer->pos);
    init_bufferpos(&buffer->screen_start);
    buffer->pos.line = buffer->screen_start.line = buffer->lines;
    buffer->line_col_offset = 0;
    buffer->line_num = buffer->byte_num = 1;
    select_reset(buffer);

    return STATUS_SUCCESS;
}

/* Loads file into buffer structure */
Status load_buffer(Buffer *buffer)
{
    Status status = STATUS_SUCCESS;

    if (!file_exists(buffer->file_info)) {
        /* If the file represented by this buffer doesn't exist
         * then the buffer content is empty */
        return reset_buffer(buffer);
    }

    FILE *input_file = fopen(buffer->file_info.rel_path, "rb");

    if (input_file == NULL) {
        return get_error(ERR_UNABLE_TO_OPEN_FILE, "Unable to open file %s for reading - %s", 
                         buffer->file_info.file_name, strerror(errno));
    } 

    char buf[FILE_BUF_SIZE];
    size_t read;
    BufferPos pos;
    init_bufferpos(&pos);
    pos.line = buffer->lines = new_line();

    if (pos.line == NULL) {
        status = get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to populate buffer");
        goto cleanup;
    }

    buffer->line_num = buffer->byte_num = 1;

    do {
        read = fread(buf, sizeof(char), FILE_BUF_SIZE, input_file);

        if (ferror(input_file)) {
            status = get_error(ERR_UNABLE_TO_READ_FILE, "Unable to read from file %s - %s", 
                               buffer->file_info.file_name, strerror(errno));
            goto cleanup;
        } 

        if (!add_to_buffer(buffer, &pos, buf, read, read < FILE_BUF_SIZE)) {
            status = get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to populate buffer");
            goto cleanup;
        }
    } while (read == FILE_BUF_SIZE) ;

    buffer->pos.line = buffer->screen_start.line = buffer->lines;

cleanup:
    fclose(input_file);

    return status;
}

/* Used when loading a file into a buffer */
static int add_to_buffer(Buffer *buffer, BufferPos *pos, const char buf[], size_t bsize, int eof)
{
    size_t idx = 0;
    Line *line = pos->line;

    while (idx < bsize) {
        if (line->length > 0 && ((line->length % LINE_ALLOC) == 0)) {
            if (!resize_line_text(line, line->length + LINE_ALLOC)) {
                return 0;
            }
        }

        /* TODO Detect and deal with CRLF and CR as well */
        if (buf[idx] == '\n') {
            line->screen_length = line_screen_length(buffer, *pos, line->length);

            if (eof && idx == (bsize - 1)) {
                return 1;
            }

            if ((line->next = new_line()) == NULL) {
                return 0;
            }

            buffer->byte_num++;
            buffer->line_num++;

            line->next->prev = line;
            pos->line = line = line->next;
        } else {
            line->text[line->length++] = buf[idx];
            buffer->byte_num++;
        }

        idx++;
    }

    return 1;
}

Status write_buffer(Buffer *buffer, const char *file_path)
{
    FileInfo *file_info = &buffer->file_info;

    size_t tmp_file_path_len = strlen(file_path) + 6 + 1;
    char *tmp_file_path = malloc(tmp_file_path_len);

    if (tmp_file_path == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to create temporary file path");
    }

    snprintf(tmp_file_path, tmp_file_path_len, "%sXXXXXX", file_path);

    int output_file = mkstemp(tmp_file_path);

    if (output_file == -1) {
        free(tmp_file_path);
        return get_error(ERR_UNABLE_TO_OPEN_FILE, "Unable to open file temporary for writing - %s",
                         strerror(errno));
    }

    Line *line = buffer->lines;
    Status status = STATUS_SUCCESS;

    while (line != NULL) {
        if (line->length > 0) {
            if (write(output_file, line->text, line->length) <= 0) {
                status = get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to write to temporary file - %s", 
                                   strerror(errno));
                break;
            }
        }

        if (write(output_file, "\n", 1) != 1) {
            status = get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to write to temporary file - %s", 
                               strerror(errno));
            break;
        }

        line = line->next;
    }

    close(output_file);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    struct stat file_stat;

    if (stat(file_path, &file_stat) == 0) {
        if (chmod(tmp_file_path, file_stat.st_mode) == -1) {
            status = get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to set file permissions - %s", 
                               strerror(errno));
            goto cleanup;
        }

        if (chown(tmp_file_path, file_stat.st_uid, file_stat.st_gid) == -1) {
            status = get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to set owner - %s",
                               strerror(errno));
            goto cleanup;
        }
    }

    if (rename(tmp_file_path, file_path) == -1) {
        status = get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to overwrite file %s - %s",
                           file_info->rel_path, strerror(errno)); 
    }

cleanup:
    if (STATUS_IS_SUCCESS(status)) {
        buffer->is_dirty = 0;
    } else {
        remove(tmp_file_path);
    }

    free(tmp_file_path);

    return status;
}

char *get_buffer_as_string(Buffer *buffer)
{
    return join_lines(buffer, "\n");
}

char *join_lines(Buffer *buffer, const char *separator)
{
    size_t byte_num = buffer->byte_num;
    size_t separator_len = strlen(separator);

    if (separator_len == 0) {
        byte_num -= (buffer->line_num - 1);
    } else if (separator_len > 1) {
        byte_num += ((buffer->line_num - 1) * (separator_len - 1));
    }

    char *str = malloc(byte_num + 1);
    RETURN_IF_NULL(str);
    char *iter = str;
    Line *line = buffer->lines;

    while (line->next != NULL) {
        if (line->length > 0) {
            memcpy(iter, line->text, line->length);
            iter += line->length;
        }

        if (separator_len > 0) {
            memcpy(iter, separator, separator_len);
            iter += separator_len;
        }

        line = line->next;
    }

    if (line->length > 0) {
        memcpy(iter, line->text, line->length);
        iter += line->length;
    }

    *iter = '\0';

    return str;
}

int buffer_is_empty(Buffer *buffer)
{
    return buffer->line_num == 1 && buffer->byte_num == 1;
}

int buffer_file_exists(Buffer *buffer)
{
    return file_exists(buffer->file_info);
}

Line *get_line_from_offset(Line *line, Direction direction, size_t offset)
{
    if (offset == 0 || line == NULL) {
        return line;
    }

    if (direction == DIRECTION_DOWN) {
        while (line->next != NULL && offset-- > 0) {
            line = line->next;
        }
    } else if (direction == DIRECTION_UP) {
        while (line->prev != NULL && offset-- > 0) {
            line = line->prev;
        }
    }

    return line;
}

int bufferpos_compare(BufferPos pos1, BufferPos pos2)
{
    if (pos1.line_no == pos2.line_no) {
        return (pos1.col_no < pos2.col_no ? -1 : pos1.col_no > pos2.col_no);
    }

    return (pos1.line_no < pos2.line_no ? -1 : pos1.line_no > pos2.line_no);
}

BufferPos bufferpos_min(BufferPos pos1, BufferPos pos2)
{
    if (bufferpos_compare(pos1, pos2) == -1) {
        return pos1;
    }

    return pos2;
}

BufferPos bufferpos_max(BufferPos pos1, BufferPos pos2)
{
    if (bufferpos_compare(pos1, pos2) == 1) {
        return pos1;
    }

    return pos2;
}

int get_selection_range(Buffer *buffer, Range *range)
{
    if (range == NULL) {
        return 0;
    } else if (!selection_started(buffer)) {
        return 0;
    }

    range->start = bufferpos_min(buffer->pos, buffer->select_start);
    range->end = bufferpos_max(buffer->pos, buffer->select_start);

    return 1;
}

int bufferpos_in_range(Range range, BufferPos pos)
{
    if (bufferpos_compare(pos, range.start) < 0 || bufferpos_compare(pos, range.end) >= 0) {
        return 0;
    }

    return 1;
}

size_t range_length(Buffer *buffer, Range range)
{
    size_t length = 1;

    while (bufferpos_compare(range.start, range.end) != 0) {
        pos_change_char(buffer, &range.start, DIRECTION_RIGHT, 0);
        length++;
    }

    return length;
}

/* TODO Consider UTF-8 punctuation and whitespace */
CharacterClass character_class(Buffer *buffer, BufferPos pos)
{
    CharInfo char_info;
    buffer->cef.char_info(&char_info, CIP_DEFAULT, pos);

    if (char_info.byte_length == 1) {
        char character = *(pos_offset_character(buffer, pos, DIRECTION_NONE, 0));

        if (isspace(character)) {
            return CCLASS_WHITESPACE;
        } else if (ispunct(character)) {
            return CCLASS_PUNCTUATION;
        }
    }

    return CCLASS_WORD;
}

const char *pos_character(Buffer *buffer)
{
    return pos_offset_character(buffer, buffer->pos, DIRECTION_NONE, 0);
}

const char *pos_offset_character(Buffer *buffer, BufferPos pos, Direction direction, size_t offset)
{
    if (!STATUS_IS_SUCCESS(pos_change_multi_char(buffer, &pos, direction, offset, 0))) {
        return "";
    }

    if (pos.offset == pos.line->length) {
        return " ";
    }

    return pos.line->text + pos.offset;
}

/* start_offset is inclusive, end_offset is exclusive */
char *get_line_segment(Line *line, size_t start_offset, size_t end_offset)
{
    if ((start_offset >= line->length || end_offset <= start_offset) &&
        /* Allow empty lines */
        end_offset - start_offset != 0) {
        return NULL;
    }

    end_offset = (end_offset > line->length ? line->length : end_offset);
    size_t bytes_to_copy = end_offset - start_offset;

    char *text = malloc(bytes_to_copy + 1);
    RETURN_IF_NULL(text);

    if (bytes_to_copy > 0) {
        memcpy(text, line->text + start_offset, bytes_to_copy);
    }

    *(text + bytes_to_copy) = '\0';

    return text;
}

/* start is inclusive, end is exclusive */
Line *clone_line_segment(Buffer *buffer, BufferPos start, BufferPos end)
{
    Line *line = start.line;

    if (start.line != end.line || ((start.offset >= line->length || end.offset <= start.offset) &&
        /* Allow empty lines */
        end.offset - start.offset != 0)) {
        return NULL;
    }

    end.offset = (end.offset > line->length ? line->length : end.offset);
    size_t bytes_to_copy = end.offset - start.offset;

    Line *clone = malloc(sizeof(Line));
    RETURN_IF_NULL(clone);
    *clone = *line;
    clone->alloc_num = (bytes_to_copy / LINE_ALLOC) + 1;
    clone->text = malloc(clone->alloc_num * LINE_ALLOC);

    if (clone->text == NULL) {
        free(clone);
        return NULL;
    }

    clone->length = bytes_to_copy;
    start.col_no = 1;
    clone->screen_length = line_screen_length(buffer, start, end.offset);

    if (bytes_to_copy > 0) {
        memcpy(clone->text, line->text + start.offset, bytes_to_copy);
    }

    return clone;
}

int bufferpos_at_line_start(BufferPos pos)
{
    return pos.offset == 0;
}

int bufferpos_at_screen_line_start(BufferPos pos, WindowInfo win_info)
{
    if (config_bool("linewrap")) {
        return ((pos.col_no - 1) % win_info.width) == 0;
    }

    return bufferpos_at_line_start(pos);
}

int bufferpos_at_line_end(BufferPos pos)
{
    return pos.line->length == pos.offset;
}

int bufferpos_at_screen_line_end(BufferPos pos, WindowInfo win_info)
{
    if (config_bool("linewrap")) {
        return (pos.col_no % win_info.width) == 0;
    }

    return bufferpos_at_line_end(pos);
}

int bufferpos_at_first_line(BufferPos pos)
{
    return pos.line->prev == NULL;
}

int bufferpos_at_last_line(BufferPos pos)
{
    return pos.line->next == NULL;
}

int bufferpos_at_buffer_start(BufferPos pos)
{
    return bufferpos_at_first_line(pos) && bufferpos_at_line_start(pos);
}

int bufferpos_at_buffer_end(BufferPos pos)
{
    return bufferpos_at_last_line(pos) && bufferpos_at_line_end(pos);
}

int bufferpos_at_buffer_extreme(BufferPos pos)
{
    return bufferpos_at_buffer_start(pos) || bufferpos_at_buffer_end(pos);
}

int move_past_buffer_extremes(BufferPos pos, Direction direction)
{
    return ((direction == DIRECTION_LEFT && bufferpos_at_buffer_start(pos)) ||
            (direction == DIRECTION_RIGHT && bufferpos_at_buffer_end(pos)));
}

static int is_selection(Direction *direction)
{
    if (direction == NULL) {
        return 0;
    }

    int is_select = *direction & DIRECTION_WITH_SELECT;
    *direction &= ~DIRECTION_WITH_SELECT;

    return is_select;
}

int selection_started(Buffer *buffer)
{
    return buffer->select_start.line != NULL;
}

static void default_movement_selection_handler(Buffer *buffer, int is_select, Direction *direction)
{
    if (is_select) {
        if (direction != NULL) {
            *direction |= DIRECTION_WITH_SELECT;
        }

        select_continue(buffer);
    } else if (selection_started(buffer)) {
        select_reset(buffer);
    }
}

/* TODO All cursor functions bellow need to be updated to consider a global cursor offset.
 * This would mean after moving from an (empty|shorter) line to a longer line the cursor 
 * would return to the global offset it was previously on instead of staying in the first column. */

/* Move cursor up or down a line keeping the offset into the line the same 
 * or as close to the original if possible */
Status pos_change_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    if (config_bool("linewrap")) {
        return pos_change_screen_line(buffer, pos, direction, is_cursor);
    }

    return pos_change_real_line(buffer, pos, direction, is_cursor);
}

static Status pos_change_real_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    if ((direction == DIRECTION_NONE) ||
        (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN))) {

        return STATUS_SUCCESS;
    }

    if (is_cursor) {
        default_movement_selection_handler(buffer, is_select, NULL);
    }

    if ((direction == DIRECTION_DOWN && bufferpos_at_last_line(*pos)) ||
        (direction == DIRECTION_UP && bufferpos_at_first_line(*pos))) {
        
        return STATUS_SUCCESS;
    }

    if (direction == DIRECTION_DOWN) {
        pos->line_no++;
        pos->line = pos->line->next;
    } else {
        pos->line_no--;
        pos->line = pos->line->prev;
    }

    pos->offset = 0;
    pos->col_no = 1;

    if (is_cursor) {
        return advance_pos_to_line_offset(buffer, pos, is_select);
    }

    return STATUS_SUCCESS;
}

/* Move cursor up or down a screen line keeping the cursor column as close to the
 * starting value as possible. For lines which don't wrap this function behaves the
 * same as pos_change_line. For lines which wrap this allows a user to scroll up or
 * down to a different part of the line displayed as a different line on the screen.
 * Therefore this function is dependent on the width of the screen. */

static Status pos_change_screen_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    if ((direction == DIRECTION_NONE) ||
        (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN))) {

        return STATUS_SUCCESS;
    }

    Direction pos_direction = (direction == DIRECTION_DOWN ? DIRECTION_RIGHT : DIRECTION_LEFT);

    if (is_cursor) {
        default_movement_selection_handler(buffer, is_select, &pos_direction);
    }

    Line *start_line = pos->line;
    size_t screen_line = screen_height_from_screen_length(buffer->win_info, pos->col_no - 1);
    size_t screen_lines = line_screen_height(buffer->win_info, pos->line);
    int break_on_hardline = screen_lines > 1 && (screen_line + DIRECTION_OFFSET(direction)) > 0 && screen_line < screen_lines;
    size_t cols, col_num;
    cols = col_num = buffer->win_info.width;
    CharInfo char_info;
    Status status;

    while (cols > 0 && cols <= col_num) {
        if (direction == DIRECTION_DOWN) {
            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, *pos);
            status = pos_change_char(buffer, pos, pos_direction, 0);
        } else {
            status = pos_change_char(buffer, pos, pos_direction, 0);
            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, *pos);
        }

        cols -= char_info.screen_length;

        if (!STATUS_IS_SUCCESS(status)) {
            return status;
        } else if (break_on_hardline && (pos->offset == 0 || pos->offset == pos->line->length)) {
           break; 
        } else if (pos->line != start_line) {
            if (break_on_hardline || pos->line->length == 0) {
                break;
            }

            break_on_hardline = 1;
            start_line = pos->line;

            if (direction == DIRECTION_DOWN) {
                cols -= (col_num - 1 - (pos->line->prev->screen_length % col_num));
            } else {
                cols -= (col_num - 1 - (pos->line->screen_length % col_num));
            }
        }
    }

    if (is_cursor) {
        return advance_pos_to_line_offset(buffer, pos, is_select);
    }

    return STATUS_SUCCESS;
}

static Status advance_pos_to_line_offset(Buffer *buffer, BufferPos *pos, int is_select)
{
    size_t global_col_offset = buffer->line_col_offset;
    size_t current_col_offset = screen_col_no(buffer->win_info, *pos) - 1;
    Direction direction = DIRECTION_RIGHT;

    if (is_select) {
        direction |= DIRECTION_WITH_SELECT;
    }

    while (current_col_offset < global_col_offset &&
           pos->offset < pos->line->length) {
        
        RETURN_IF_FAIL(pos_change_char(buffer, pos, direction, 1));
        current_col_offset++;
    }

    buffer->line_col_offset = global_col_offset;

    return STATUS_SUCCESS;
}

Status pos_change_multi_line(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset, int is_cursor)
{
    if (offset == 0 || direction == DIRECTION_NONE) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = pos_change_line(buffer, pos, direction, is_cursor);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

/* Move cursor a character to the left or right */
Status pos_change_char(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    if ((direction == DIRECTION_NONE) ||
        (!(direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT))) {
    
        return STATUS_SUCCESS;
    }

    if (is_cursor) {
        if (is_select) {
            if (!move_past_buffer_extremes(*pos, direction)) {
                select_continue(buffer);
            }
        } else if (selection_started(buffer)) {
            Range select_range;
            BufferPos new_pos;

            get_selection_range(buffer, &select_range);

            if (direction == DIRECTION_LEFT) {
                new_pos = select_range.start;
            } else {
                new_pos = select_range.end;
            }

            select_reset(buffer);

            return pos_to_bufferpos(buffer, new_pos);
        }
    }

    if (move_past_buffer_extremes(*pos, direction)) {
        return STATUS_SUCCESS;
    }

    Line *line = pos->line;

    if (pos->offset == 0 && direction == DIRECTION_LEFT) {
        pos->line = line = line->prev; 
        pos->offset = line->length == 0 ? 0 : line->length;
        pos->line_no--;
        pos->col_no = line->screen_length + 1;
    } else if ((pos->offset == line->length || line->length == 0) && direction == DIRECTION_RIGHT) {
        pos->line = line = line->next; 
        pos->offset = 0;
        pos->line_no++;
        pos->col_no = 1;
    } else {
        CharInfo char_info;

        if (direction == DIRECTION_LEFT) {
            size_t byte_offset = buffer->cef.previous_char_offset(pos->line->text + pos->offset, pos->offset);
            pos->offset -= byte_offset;

            if (*(pos->line->text + pos->offset) == '\t') {
                BufferPos line_start = to_line_start(*pos);
                pos->col_no = line_screen_length(buffer, line_start, pos->offset) + 1;
            } else {
                BufferPos tmp = *pos;

                for (size_t k = 0; k < byte_offset; k += char_info.byte_length) {
                    tmp.offset += k;
                    buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, tmp);
                    tmp.col_no += char_info.screen_length;
                    pos->col_no -= char_info.screen_length;
                }
            }
        } else {
            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, *pos);
            pos->col_no += char_info.screen_length;
            pos->offset += char_info.byte_length;
        }
    }

    if (is_cursor) {
        update_line_col_offset(buffer, pos);
    }

    return STATUS_SUCCESS;
}

Status pos_change_multi_char(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset, int is_cursor)
{
    if (offset == 0 || direction == DIRECTION_NONE) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = pos_change_char(buffer, pos, direction, is_cursor);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

static void update_line_col_offset(Buffer *buffer, BufferPos *pos)
{
    if (config_bool("linewrap")) {
        /* Windowinfo may not be initialised when the error buffer is populated,
         * but line_col_offset isn't needed in this case anyway. */
        if (buffer->win_info.width > 0) {
            buffer->line_col_offset = (pos->col_no - 1) % buffer->win_info.width;
        }
    } else {
        buffer->line_col_offset = pos->col_no - 1;
    }
}

Status pos_to_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    if (config_bool("linewrap")) {
        return bpos_to_screen_line_start(buffer, pos, is_select, is_cursor);
    }

    return bpos_to_line_start(buffer, pos, is_select, is_cursor);
}

static BufferPos to_line_start(BufferPos pos)
{
    bpos_to_line_start(NULL, &pos, 0, 0);
    return pos;
}

Status bpos_to_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    if (is_cursor) {
        Direction direction = DIRECTION_LEFT;
        default_movement_selection_handler(buffer, is_select, &direction);
    }

    if (pos->offset == 0) {
        return STATUS_SUCCESS;
    }

    if (is_cursor) {
        buffer->line_col_offset = 0;
    }

    pos->offset = 0;
    pos->col_no = 1;

    return STATUS_SUCCESS;
}

Status bpos_to_screen_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    Direction direction = DIRECTION_LEFT;

    if (is_cursor) {
        default_movement_selection_handler(buffer, is_select, &direction);
    }

    if (pos->offset == 0) {
        return STATUS_SUCCESS;
    }

    do {
        RETURN_IF_FAIL(pos_change_char(buffer, pos, direction, is_cursor));
    } while (pos->offset > 0 && !bufferpos_at_screen_line_start(*pos, buffer->win_info)) ;

    return STATUS_SUCCESS;
}

Status pos_to_line_end(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;

    if (pos->offset == pos->line->length) {
        return STATUS_SUCCESS;
    } else if (!config_bool("linewrap")) {
        pos->offset = pos->line->length;
        pos->col_no = pos->line->screen_length + 1;
        return STATUS_SUCCESS;
    }

    do {
        RETURN_IF_FAIL(pos_change_char(buffer, pos, direction, 1));
    } while (pos->offset != pos->line->length && 
             !bufferpos_at_screen_line_end(*pos, buffer->win_info)) ;

    return STATUS_SUCCESS;
}

Status pos_to_next_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    if (is_select) {
        while (character_class(buffer, *pos) == CCLASS_WHITESPACE) {
            RETURN_IF_FAIL(pos_change_char(buffer, pos, direction, 1));

            if (bufferpos_at_line_start(buffer->pos)) {
                return STATUS_SUCCESS;
            }
        }
    }

    CharacterClass start_class = character_class(buffer, *pos);

    do {
        status = pos_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    } while (!bufferpos_at_buffer_end(buffer->pos) &&
             start_class == character_class(buffer, *pos));

    if (is_select) {
        return STATUS_SUCCESS;
    }

    while (!bufferpos_at_line_end(buffer->pos) &&
           character_class(buffer, *pos) == CCLASS_WHITESPACE) {

        status = pos_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

Status pos_to_prev_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_LEFT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    do {
        status = pos_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);

        if (bufferpos_at_line_end(buffer->pos)) {
            return STATUS_SUCCESS;
        }
    } while (character_class(buffer, *pos) == CCLASS_WHITESPACE);

    CharacterClass start_class = character_class(buffer, *pos);
    BufferPos look_ahead = buffer->pos;

    while (!bufferpos_at_line_start(buffer->pos)) {
        RETURN_IF_FAIL(pos_change_char(buffer, &look_ahead, DIRECTION_LEFT, 0));

        if (start_class != character_class(buffer, look_ahead)) {
            break;
        }

        status = pos_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

Status pos_to_buffer_start(Buffer *buffer, int is_select)
{
    default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    pos->line = buffer->lines;
    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;

    return STATUS_SUCCESS;
}

Status pos_to_buffer_end(Buffer *buffer, int is_select)
{
    default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    Line *next;

    while ((next = pos->line->next) != NULL) {
        pos->line = next;
        pos->line_no++;
    }

    pos->offset = pos->line->length;
    pos->col_no = pos->line->screen_length + 1;

    return STATUS_SUCCESS;
}

Status pos_to_bufferpos(Buffer *buffer, BufferPos pos)
{
    buffer->pos = pos; 
    return STATUS_SUCCESS;
}

Status pos_change_page(Buffer *buffer, Direction direction)
{
    int is_select = is_selection(&direction);

    if (bufferpos_at_first_line(buffer->pos) && direction == DIRECTION_UP) {
        return STATUS_SUCCESS;
    }

    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status = pos_change_multi_line(buffer, pos, direction, buffer->win_info.height - 1, 1);

    RETURN_IF_FAIL(status);

    if (buffer->screen_start.line != buffer->pos.line) {
        buffer->screen_start = buffer->pos;
        RETURN_IF_FAIL(bpos_to_screen_line_start(buffer, &buffer->screen_start, 0, 0));
    }

    return STATUS_SUCCESS;
}

Status insert_character(Buffer *buffer, const char *character)
{
    size_t char_len = 0;
    
    if (character != NULL) {
        char_len = strnlen(character, 5);
    }

    if (char_len == 0 || char_len > 4) {
        return get_error(ERR_INVALID_CHARACTER, "Invalid character %s", character);
    }

    Range range;

    if (get_selection_range(buffer, &range)) {
        Status status = delete_range(buffer, range);
        RETURN_IF_FAIL(status);
    }

    BufferPos *pos = &buffer->pos;

    if (!resize_line_text_if_req(pos->line, pos->line->length + char_len)) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert character %s", character);
    }

    if (pos->line->length > 0 && pos->offset < pos->line->length) {
        memmove(pos->line->text + pos->offset + char_len, pos->line->text + pos->offset, pos->line->length - pos->offset);
    }

    memcpy(pos->line->text + pos->offset, character, char_len);
    pos->line->length += char_len;
    pos->line->screen_length = pos->col_no - 1 + line_screen_length(buffer, *pos, pos->line->length);
    buffer->byte_num += char_len;
    buffer->is_dirty = 1;
    RETURN_IF_FAIL(pos_change_char(buffer, pos, DIRECTION_RIGHT, 1));

    return STATUS_SUCCESS;
}

Status insert_string(Buffer *buffer, const char *string, size_t string_length, int advance_cursor)
{
    if (string == NULL) {
        return get_error(ERR_INVALID_CHARACTER, "Cannot insert NULL string");
    } else if (string_length == 0) {
        return STATUS_SUCCESS;
    }

    Range range;

    if (get_selection_range(buffer, &range)) {
        Status status = delete_range(buffer, range);
        RETURN_IF_FAIL(status);
    }

    BufferPos *pos = &buffer->pos;

    if (!resize_line_text_if_req(pos->line, pos->line->length + string_length)) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert string");
    }

    if (pos->line->length > 0 && pos->offset < pos->line->length) {
        memmove(pos->line->text + pos->offset + string_length, pos->line->text + pos->offset, pos->line->length - pos->offset);
    }

    memcpy(pos->line->text + pos->offset, string, string_length);
    pos->line->length += string_length;
    pos->line->screen_length = pos->col_no - 1 + line_screen_length(buffer, *pos, pos->line->length);
    buffer->byte_num += string_length;
    buffer->is_dirty = 1;

    if (advance_cursor) {
        size_t end_offset = pos->offset + string_length;

        while (pos->offset < end_offset) {
            RETURN_IF_FAIL(pos_change_char(buffer, pos, DIRECTION_RIGHT, 1));
        }
    }

    return STATUS_SUCCESS;
} 

Status delete_character(Buffer *buffer)
{
    Range range;

    if (get_selection_range(buffer, &range)) {
        return delete_range(buffer, range);
    }

    BufferPos *pos = &buffer->pos;
    Line *line = pos->line;

    if (pos->offset == line->length) {
        if (line->next == NULL) {
            return STATUS_SUCCESS;
        }

        RETURN_IF_FAIL(insert_string(buffer, line->next->text, line->next->length, 0));
        RETURN_IF_FAIL(delete_line(buffer, line->next));

        return STATUS_SUCCESS;
    }

    CharInfo char_info;
    buffer->cef.char_info(&char_info, CIP_DEFAULT, *pos);

    if (pos->offset != (line->length - 1)) {
        memmove(line->text + pos->offset, line->text + pos->offset + char_info.byte_length, line->length - pos->offset);
    }

    line->length -= char_info.byte_length;
    line->screen_length = pos->col_no - 1 + line_screen_length(buffer, *pos, pos->line->length);
    buffer->byte_num -= char_info.byte_length;
    buffer->is_dirty = 1;

    /* TODO Raise error here? Failing to shrink memory doesn't have an adverse effect so
     * don't raise an error. It does hint that future {m,re}allocs could likely fail however. */
    resize_line_text_if_req(pos->line, pos->line->length);

    return STATUS_SUCCESS;
}

Status delete_line(Buffer *buffer, Line *line)
{
    Status status = STATUS_SUCCESS;

    if (buffer == NULL || line == NULL) {
        return STATUS_SUCCESS;
    }

    buffer->byte_num -= line->length + 1;
    buffer->line_num--;
    buffer->is_dirty = 1;

    if (line->prev != NULL) {
        line->prev->next = line->next;
    } 

    if (line->next != NULL) {
        line->next->prev = line->prev;
    }

    if (buffer->pos.line == line) {
        if (line->next != NULL) {
            buffer->pos.line = line->next;
        } else {
            buffer->pos.line = line->prev;
        }    
    }

    if (buffer->screen_start.line == line) {
        if (line->next != NULL) {
            buffer->screen_start.line = line->next;
        } else {
            buffer->screen_start.line = line->prev;
        }    
    }

    if (buffer->lines == line) {
        if (line->next != NULL) {
            buffer->lines = line->next;
        } else {
            status = reset_buffer(buffer);
        }    
    }

    free_line(line);

    return status;
}

Status insert_line(Buffer *buffer)
{
    Range range;

    if (get_selection_range(buffer, &range)) {
        Status status = delete_range(buffer, range);
        RETURN_IF_FAIL(status);
    }

    BufferPos *pos = &buffer->pos;
    size_t line_length = pos->line->length - pos->offset;

    Line *line = new_sized_line(line_length);

    if (line == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert line");
    }

    if (line_length > 0) {
        memcpy(line->text, pos->line->text + pos->offset, line_length); 
        line->length = line_length;
        BufferPos line_start =  to_line_start(*pos);
        line_start.line = line;
        line->screen_length = line_screen_length(buffer, line_start, line_length);
        pos->line->screen_length = pos->col_no - 1;
        pos->line->length = pos->offset;
        resize_line_text_if_req(pos->line, pos->line->length);
    }

    line->next = pos->line->next;
    line->prev = pos->line;
    pos->line->next = line;

    if (line->next != NULL) {
        line->next->prev = line;
    }

    pos->line = line;
    pos->offset = 0;
    pos->line_no++;
    pos->col_no = 1;
    buffer->byte_num++;
    buffer->line_num++;
    buffer->is_dirty = 1;

    return STATUS_SUCCESS;
}

Status select_continue(Buffer *buffer)
{
    if (buffer->select_start.line == NULL) {
        buffer->select_start = buffer->pos;
    }

    return STATUS_SUCCESS;
}

Status select_reset(Buffer *buffer)
{
    buffer->select_start.line = NULL;
    buffer->select_start.offset = 0;

    return STATUS_SUCCESS;
}

/* start_offset is inclusive, end_offset is exclusive */
static Status delete_line_segment(Buffer *buffer, BufferPos start, BufferPos end)
{
    if (start.line != end.line || start.line->length == 0 ||
        start.offset >= start.line->length || end.offset <= start.offset) {
        return STATUS_SUCCESS; 
    }

    Line *line = start.line;
    end.offset = (end.offset > line->length ? line->length : end.offset);
    size_t bytes_to_move = line->length - end.offset;

    if (bytes_to_move > 0) {
        memmove(line->text + start.offset, line->text + end.offset, bytes_to_move);
    }

    line->length -= (end.offset - start.offset);
    line->screen_length = start.col_no - 1 + line_screen_length(buffer, start, line->length);
    buffer->byte_num -= (end.offset - start.offset);
    buffer->is_dirty = 1;

    /* Don't check for success for reasons mentioned in delete_character */
    resize_line_text_if_req(line, line->length);

    return STATUS_SUCCESS;
}

Status delete_range(Buffer *buffer, Range range)
{
    select_reset(buffer);
    buffer->pos = range.start;

    int is_single_line = (range.start.line == range.end.line);

    BufferPos end = range.start;
    end.offset = end.line->length;

    if (is_single_line) {
        end = range.end;
    }

    Status status = delete_line_segment(buffer, range.start, end);

    if (is_single_line || !STATUS_IS_SUCCESS(status)) {
        return status;
    }

    Line *line = range.start.line->next;
    Line *next;

    while (line != range.end.line) {
        next = line->next;
        status = delete_line(buffer, line);

        RETURN_IF_FAIL(status);

        line = next;
    }

    status = insert_string(buffer, range.end.line->text + range.end.offset, range.end.line->length - range.end.offset, 0);
    RETURN_IF_FAIL(status);

    status = delete_line(buffer, range.end.line);

    return status;
}

Status select_all_text(Buffer *buffer)
{
    Line *line = buffer->pos.line;
    size_t end_line_no = buffer->pos.line_no;

    while (line->next != NULL) {
        line = line->next;
        end_line_no++;
    }

    buffer->select_start = (BufferPos) { 
        .line = line, 
        .offset = line->length, 
        .line_no = end_line_no, 
        .col_no = line->screen_length + 1 
    };

    buffer->pos = (BufferPos) { 
        .line = buffer->lines, 
        .offset = 0,
        .line_no = 1,
        .col_no = 1 
    };

    return STATUS_SUCCESS;
}

Status copy_selected_text(Buffer *buffer, TextSelection **text_selection)
{
    if (buffer == NULL || text_selection == NULL) {
        return STATUS_SUCCESS;
    }

    Range range;

    if (!get_selection_range(buffer, &range)) {
        *text_selection = NULL;
        return STATUS_SUCCESS;
    }

    *text_selection = new_textselection(buffer, range); 

    if (*text_selection == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy selected text");
    }

    return STATUS_SUCCESS;
}

Status cut_selected_text(Buffer *buffer, TextSelection **text_selection)
{
    if (buffer == NULL || text_selection == NULL) {
        return STATUS_SUCCESS;
    }

    Range range;

    if (!get_selection_range(buffer, &range)) {
        return STATUS_SUCCESS;
    }
    
    Status status = copy_selected_text(buffer, text_selection);

    if (!STATUS_IS_SUCCESS(status) || text_selection == NULL) {
        return status;
    }

    return delete_range(buffer, range);
}

Status insert_textselection(Buffer *buffer, TextSelection *text_selection)
{
    if (buffer == NULL || text_selection == NULL) {
        return STATUS_SUCCESS;
    }

    Range range;

    if (get_selection_range(buffer, &range)) {
        RETURN_IF_FAIL(delete_range(buffer, range));
    }

    if (text_selection->type == TST_STRING) {
        return insert_string(buffer, text_selection->text.string, strlen(text_selection->text.string), 1);
    }

    Line *line = text_selection->text.lines;
    Line *buf_line = buffer->pos.line;

    RETURN_IF_FAIL(insert_string(buffer, line->text, line->length, 1));
    RETURN_IF_FAIL(insert_line(buffer));

    Line *end_line = buffer->pos.line;
    line = line->next;

    while (line->next != NULL) {
        buf_line->next = clone_line(line); 
    
        if (buf_line->next == NULL) {
            if (buf_line != end_line) {
                buf_line->next = end_line; 
                end_line->prev = buf_line;
            }

            return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert all text");
        }

        buf_line->next->prev = buf_line;
        buf_line = buf_line->next;
        buffer->pos.line_no++;
        buffer->line_num++;
        buffer->byte_num += buf_line->length + 1;
        line = line->next;
    }

    buf_line->next = end_line; 
    end_line->prev = buf_line;

    return insert_string(buffer, line->text, line->length, 1);
}

Status delete_word(Buffer *buffer)
{
    Range range;

    if (get_selection_range(buffer, &range)) {
        return delete_range(buffer, range);
    }

    if (bufferpos_at_buffer_end(buffer->pos)) {
        return STATUS_SUCCESS;
    }

    BufferPos select_start = buffer->pos;
    RETURN_IF_FAIL(pos_to_next_word(buffer, 0));
    buffer->select_start = select_start;

    get_selection_range(buffer, &range);
    RETURN_IF_FAIL(delete_range(buffer, range));

    return STATUS_SUCCESS;
}

Status delete_prev_word(Buffer *buffer)
{
    Range range;

    if (get_selection_range(buffer, &range)) {
        return delete_range(buffer, range);
    }

    if (bufferpos_at_buffer_start(buffer->pos)) {
        return STATUS_SUCCESS;
    }

    BufferPos select_start = buffer->pos;
    RETURN_IF_FAIL(pos_to_prev_word(buffer, 0));
    buffer->select_start = select_start;

    get_selection_range(buffer, &range);
    RETURN_IF_FAIL(delete_range(buffer, range));

    return STATUS_SUCCESS;
}
