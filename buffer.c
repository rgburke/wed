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
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "status.h"
#include "display.h"
#include "file.h"

static Line *add_to_buffer(const char *, size_t, Line *, int);
static int is_selection(Direction *);
static void default_movement_selection_handler(Buffer *, int, Direction *);
static void update_line_col_offset(Buffer *, BufferPos *);
static Status advance_pos_to_line_offset(Buffer *, BufferPos *, int);

Buffer *new_buffer(FileInfo file_info)
{
    Buffer *buffer = alloc(sizeof(Buffer));

    buffer->file_info = file_info;
    init_bufferpos(&buffer->pos);
    init_bufferpos(&buffer->screen_start);
    init_bufferpos(&buffer->select_start);
    buffer->lines = NULL;
    buffer->next = NULL;
    buffer->line_col_offset = 0;

    return buffer;
}

Buffer *new_empty_buffer(void)
{
    FileInfo file_info;
    init_empty_fileinfo(&file_info);
    Buffer *buffer = new_buffer(file_info); 
    buffer->lines = buffer->pos.line = buffer->screen_start.line = new_line();
    return buffer;
}

void free_buffer(Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    free_fileinfo(buffer->file_info);

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

    Line *line = alloc(sizeof(Line));
    line->text = alloc(alloc_num * LINE_ALLOC);
    line->alloc_num = alloc_num;
    line->length = 0; 
    line->screen_length = 0;
    line->is_dirty = 0;
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

    return 1;
}

/* TODO When editing functionality is added this function
 * will need to expand and shrink a line.
 * Also need to consider adding a function to determine
 * how much to expand or shrink by.
 * Also pick better function name. */
void resize_line_text_if_req(Line *line, size_t new_size)
{
    if (line == NULL) {
        return;
    }

    size_t allocated = line->alloc_num * LINE_ALLOC;

    if (new_size > allocated) {
        resize_line_text(line, new_size);  
    } else if (new_size < (allocated - LINE_ALLOC)) {
        resize_line_text(line, new_size);
    }
}

void resize_line_text(Line *line, size_t new_size)
{
    if (line == NULL) {
        return;
    }

    line->alloc_num = (new_size / LINE_ALLOC) + 1;

    line->text = ralloc(line->text, line->alloc_num * LINE_ALLOC);
}

/* Loads file into buffer structure */
Status load_buffer(Buffer *buffer)
{
    if (!buffer->file_info.exists) {
        /* If the file represented by this buffer doesn't exist
         * then the buffer content is empty */
        buffer->lines = buffer->pos.line = buffer->screen_start.line = new_line();
        return STATUS_SUCCESS;
    }

    FILE *input_file = fopen(buffer->file_info.rel_path, "rb");

    if (input_file == NULL) {
        return raise_param_error(ERR_UNABLE_TO_OPEN_FILE, STR_VAL(buffer->file_info.file_name));
    } 

    char buf[FILE_BUF_SIZE];
    size_t read;
    Line *line = buffer->lines = new_line();

    while ((read = fread(buf, sizeof(char), FILE_BUF_SIZE, input_file)) > 0) {
        if (read != FILE_BUF_SIZE && ferror(input_file)) {
            fclose(input_file);
            return raise_param_error(ERR_UNABLE_TO_READ_FILE, STR_VAL(buffer->file_info.file_name));
        } 

        line = add_to_buffer(buf, read, line, read < FILE_BUF_SIZE);
    }

    fclose(input_file);

    if (buffer->lines) {
        buffer->pos.line = buffer->screen_start.line = buffer->lines;
    }

    return STATUS_SUCCESS;
}

/* Used when loading a file into a buffer */
static Line *add_to_buffer(const char buffer[], size_t bsize, Line *line, int eof)
{
    size_t idx = 0;

    while (idx < bsize) {
        if (line->length > 0 && ((line->length % LINE_ALLOC) == 0)) {
            resize_line_text(line, line->length + LINE_ALLOC);
        }

        if (buffer[idx] == '\n' && !(eof && idx == (bsize - 1))) {
                line->next = new_line();
                line->next->prev = line;
                line = line->next;
        } else {
            line->screen_length += byte_screen_length(buffer[idx], line, line->length);
            line->text[line->length++] = buffer[idx];
        }

        idx++;
    }

    return line;
}

size_t get_pos_line_number(Buffer *buffer)
{
    return get_bufferpos_line_number(buffer->pos);
}

size_t get_bufferpos_line_number(BufferPos pos)
{
    size_t line_num = 1;
    Line *line = pos.line;

    while ((line = line->prev) != NULL) {
        line_num++;
    }

    return line_num;
}

size_t get_pos_col_number(Buffer *buffer)
{
    BufferPos pos = buffer->pos;
    size_t col_no = line_screen_length(pos.line, 0, pos.offset);
    return col_no + 1;
}

Line *get_line_from_offset(Line *line, Direction direction, size_t offset)
{
    if (offset == 0 || line == NULL) {
        return line;
    }

    Line *next;

    if (direction == DIRECTION_DOWN) {
        while ((next = line->next) != NULL && offset-- > 0) {
            line = next;
        }
    } else if (direction == DIRECTION_UP) {
        while ((next = line->prev) != NULL && offset-- > 0) {
            line = next;
        }
    }

    return line;
}

int offset_compare(size_t offset1, size_t offset2)
{
    return (offset1 < offset2 ? -1 : offset1 > offset2);
}

int bufferpos_compare(BufferPos pos1, BufferPos pos2)
{
    if (pos1.line == pos2.line) {
        return offset_compare(pos1.offset, pos2.offset);
    }

    size_t pos1_line_no = get_bufferpos_line_number(pos1);
    size_t pos2_line_no = get_bufferpos_line_number(pos2);

    return (pos1_line_no < pos2_line_no ? -1 : 1);
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

/* TODO Consider UTF-8 punctuation and whitespace */
CharacterClass character_class(const char *character)
{
    if (char_byte_length(*character) == 1) {
        if (isspace(*character)) {
            return CCLASS_WHITESPACE;
        } else if (ispunct(*character)) {
            return CCLASS_PUNCTUATION;
        }
    }

    return CCLASS_WORD;
}

const char *pos_character(Buffer *buffer)
{
    return pos_offset_character(buffer, DIRECTION_NONE, 0);
}

const char *pos_offset_character(Buffer *buffer, Direction direction, size_t offset)
{
    BufferPos pos = buffer->pos;

    if (!is_success(pos_change_multi_char(buffer, &pos, direction, offset, 0))) {
        return "";
    }

    if (pos.offset == pos.line->length) {
        return " ";
    }

    return pos.line->text + pos.offset;
}

int bufferpos_at_line_start(BufferPos pos)
{
    return pos.offset == 0;
}

int bufferpos_at_line_end(BufferPos pos)
{
    return pos.line->length == pos.offset;
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
Status pos_change_line(Buffer *buffer, BufferPos *pos, Direction direction)
{
    (void)buffer;
    Line *line = pos->line;

    if (direction == DIRECTION_NONE ||
        (direction == DIRECTION_DOWN && line->next == NULL) ||
        (direction == DIRECTION_UP && line->prev == NULL)) {

        return STATUS_SUCCESS;
    }

    size_t current_screen_offset = line_screen_length(line, 0, pos->offset);
    size_t new_screen_offset = 0;

    pos->line = line = (direction == DIRECTION_DOWN ? line->next : line->prev);
    pos->offset = -1;

    while (++pos->offset < line->length && new_screen_offset < current_screen_offset) {
        new_screen_offset += byte_screen_length(line->text[pos->offset], line, pos->offset); 
    }

    return STATUS_SUCCESS;
}

Status pos_change_muti_line(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset)
{
    if (offset == 0 || direction == DIRECTION_NONE) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = pos_change_line(buffer, pos, direction);

        if (!is_success(status)) {
            return status;
        }
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
            BufferPos new_pos;

            if (direction == DIRECTION_LEFT) {
                new_pos = bufferpos_min(buffer->select_start, buffer->pos);
            } else {
                new_pos = bufferpos_max(buffer->select_start, buffer->pos);
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
    } else if ((pos->offset == line->length || line->length == 0) && direction == DIRECTION_RIGHT) {
        pos->line = line = line->next; 
        pos->offset = 0;
    } else {
        pos->offset += DIRECTION_OFFSET(direction);
    }

    /* Ensure we're not on a continuation byte */
    while (!byte_screen_length(line->text[pos->offset], line, pos->offset) &&
           pos->offset < line->length &&
           (pos->offset += DIRECTION_OFFSET(direction)) > 0) ;

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

        if (!is_success(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

/* Move cursor up or down a screen line keeping the cursor column as close to the
 * starting value as possible. For lines which don't wrap this function behaves the
 * same as pos_change_line. For lines which wrap this allows a user to scroll up or
 * down to a different part of the line displayed as a different line on the screen.
 * Therefore this function is dependent on the width of the screen. */

Status pos_change_screen_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
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
    size_t screen_line = line_pos_screen_height(*pos);
    size_t screen_lines = line_screen_height(pos->line);
    int break_on_hardline = screen_lines > 1 && (screen_line + DIRECTION_OFFSET(direction)) > 0 && screen_line < screen_lines;
    size_t cols, col_num;
    cols = col_num = editor_screen_width();
    Status status;

    while (cols > 0 && cols <= col_num) {
        cols -= byte_screen_length(pos->line->text[pos->offset], pos->line, pos->offset);
        status = pos_change_char(buffer, pos, pos_direction, 0);

        if (!is_success(status)) {
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

Status pos_change_multi_screen_line(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset, int is_cursor)
{
    if (offset == 0 || direction == DIRECTION_NONE) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = pos_change_screen_line(buffer, pos, direction, is_cursor);

        if (!is_success(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

static void update_line_col_offset(Buffer *buffer, BufferPos *pos)
{
    buffer->line_col_offset = screen_col_no(*pos);
}

static Status advance_pos_to_line_offset(Buffer *buffer, BufferPos *pos, int is_select)
{
    size_t global_col_offset = buffer->line_col_offset;
    size_t current_col_offset = screen_col_no(*pos);
    Direction direction = DIRECTION_RIGHT;
    Status status;

    if (is_select) {
        direction |= DIRECTION_WITH_SELECT;
    }

    while (current_col_offset < global_col_offset &&
           pos->offset < pos->line->length) {
        
        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }

        current_col_offset++;
    }

    buffer->line_col_offset = global_col_offset;

    return STATUS_SUCCESS;
}

Status pos_to_screen_line_start(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_LEFT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;

    if (pos->offset == 0) {
        return STATUS_SUCCESS;
    }

    size_t screen_width = editor_screen_width();
    size_t col_index;
    Status status;

    do {
        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }

        col_index = screen_col_no(*pos);
    } while (pos->offset > 0 && (col_index % screen_width) != 0) ;

    return STATUS_SUCCESS;
}

Status pos_to_screen_line_end(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;

    if (pos->offset == pos->line->length) {
        return STATUS_SUCCESS;
    }

    size_t screen_width = editor_screen_width();
    size_t col_index;
    Status status;

    do {
        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }

        col_index = screen_col_no(*pos);
    } while (pos->offset != pos->line->length && (col_index % screen_width) != (screen_width - 1)) ;

    return STATUS_SUCCESS;
}

Status pos_to_next_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    CharacterClass start_class = character_class(pos_character(buffer));

    do {
        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }
    } while (!bufferpos_at_buffer_end(buffer->pos) &&
             start_class == character_class(pos_character(buffer)));

    while (!bufferpos_at_buffer_extreme(buffer->pos) &&
           character_class(pos_character(buffer)) == CCLASS_WHITESPACE) {

        if (bufferpos_at_line_end(buffer->pos)) {
            break;
        }

        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }
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

        if (!is_success(status)) {
            return status;
        }
    } while (!bufferpos_at_buffer_start(buffer->pos) &&
             character_class(pos_character(buffer)) == CCLASS_WHITESPACE);

    CharacterClass start_class = character_class(pos_character(buffer));

    while (!bufferpos_at_buffer_start(buffer->pos) &&
           start_class == character_class(pos_offset_character(buffer, DIRECTION_LEFT, 1))) {

        status = pos_change_char(buffer, pos, direction, 1);

        if (!is_success(status)) {
            return status;
        }
    }

    return STATUS_SUCCESS;
}

Status pos_to_buffer_start(Buffer *buffer, int is_select)
{
    default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    pos->line = buffer->lines;
    pos->offset = 0;

    return STATUS_SUCCESS;
}

Status pos_to_buffer_end(Buffer *buffer, int is_select)
{
    default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    Line *next;

    while ((next = pos->line->next) != NULL) {
        pos->line = next;
    }

    pos->offset = pos->line->length;

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
    Status status = pos_change_multi_screen_line(buffer, pos, direction, editor_screen_height() - 1, 1);

    if (!is_success(status)) {
        return status;
    }

    if (buffer->screen_start.line != buffer->pos.line) {
        buffer->screen_start.line = buffer->pos.line;
        buffer->screen_start.line->is_dirty = DRAW_LINE_REFRESH_DOWN;
    }

    return STATUS_SUCCESS;
}

Status insert_character(Buffer *buffer, char *character)
{
    size_t char_len = 0;
    
    if (character != NULL) {
        char_len = strnlen(character, 7);
    }

    if (char_len == 0 || char_len > 6) {
        return raise_param_error(ERR_INVALID_CHARACTER, STR_VAL(character));     
    }

    BufferPos *pos = &buffer->pos;

    resize_line_text_if_req(pos->line, pos->line->length + char_len);

    if (pos->line->length > 0 && pos->offset < pos->line->length) {
        memmove(pos->line->text + pos->offset + char_len, pos->line->text + pos->offset, pos->line->length - pos->offset);
    }

    size_t start_screen_height = line_screen_height(pos->line);

    while (*character && char_len--) {
        pos->line->screen_length += byte_screen_length(*character, pos->line, pos->offset);
        pos->line->text[pos->offset++] = *character++;
        pos->line->length++;
    }

    size_t end_screen_height = line_screen_height(pos->line);

    if (end_screen_height > start_screen_height) {
        pos->line->is_dirty = DRAW_LINE_REFRESH_DOWN;
    } else {
        pos->line->is_dirty = DRAW_LINE_EXTENDED;
    }

    return STATUS_SUCCESS;
}

Status insert_string(Buffer *buffer, char *string, size_t string_length, int advance_cursor)
{
    if (string == NULL) {
        return raise_param_error(ERR_INVALID_CHARACTER, STR_VAL(string));     
    } else if (string_length == 0) {
        return STATUS_SUCCESS;
    }

    BufferPos *pos = &buffer->pos;

    resize_line_text_if_req(pos->line, pos->line->length + string_length);

    if (pos->line->length > 0 && pos->offset < pos->line->length) {
        memmove(pos->line->text + pos->offset + string_length, pos->line->text + pos->offset, pos->line->length - pos->offset);
    }

    size_t start_offset = pos->offset;
    size_t start_screen_height = line_screen_height(pos->line);

    while (string_length--) {
        pos->line->screen_length += byte_screen_length(*string, pos->line, pos->offset);
        pos->line->text[pos->offset++] = *string++;
        pos->line->length++;
    }

    size_t end_screen_height = line_screen_height(pos->line);

    if (!advance_cursor) {
        pos->offset = start_offset;
    }

    if (end_screen_height > start_screen_height) {
        pos->line->is_dirty = DRAW_LINE_REFRESH_DOWN;
    } else {
        pos->line->is_dirty = DRAW_LINE_EXTENDED;
    }

    return STATUS_SUCCESS;
} 

Status delete_character(Buffer *buffer)
{
    BufferPos *pos = &buffer->pos;
    Line *line = pos->line;

    if (pos->offset == line->length) {
        if (line->next == NULL) {
            return STATUS_SUCCESS;
        }

        Status status = insert_string(buffer, line->next->text, line->next->length, 0);

        if (!is_success(status)) {
            return status;
        }

        status = delete_line(buffer, line->next);

        if (!is_success(status)) {
            return status;
        }

        line->is_dirty = DRAW_LINE_REFRESH_DOWN;

        return STATUS_SUCCESS;
    }

    size_t char_byte_len = char_byte_length(line->text[pos->offset]);
    size_t screen_length = byte_screen_length(line->text[pos->offset], line, pos->offset);

    if (pos->offset != (line->length - 1)) {
        memmove(line->text + pos->offset, line->text + pos->offset + char_byte_len, line->length - pos->offset);
    }

    line->length -= char_byte_len;
    line->screen_length -= screen_length; 
    pos->line->is_dirty = DRAW_LINE_SHRUNK;

    resize_line_text_if_req(pos->line, pos->line->length);

    return STATUS_SUCCESS;
}

Status delete_line(Buffer *buffer, Line *line)
{
    if (buffer == NULL || line == NULL) {
        return STATUS_SUCCESS;
    }

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

    if (buffer->screen_start.line == NULL) {
        if (line->next != NULL) {
            buffer->screen_start.line = line->next;
        } else {
            buffer->screen_start.line = line->prev;
        }    
    }

    free_line(line);

    return STATUS_SUCCESS;
}

Status insert_line(Buffer *buffer)
{
    BufferPos *pos = &buffer->pos;
    size_t line_length = pos->line->length - pos->offset;

    Line *line = new_sized_line(line_length);

    if (line_length > 0) {
        memcpy(line->text, pos->line->text + pos->offset, line_length); 
        line->length = line_length;
        line->screen_length = line_screen_length(pos->line, pos->offset, pos->line->length);
        pos->line->screen_length -= line->screen_length;
        pos->line->length = pos->offset;
    }

    line->next = pos->line->next;
    line->prev = pos->line;
    pos->line->next = line;

    if (line->next != NULL) {
        line->next->prev = line;
    }

    pos->line->is_dirty = DRAW_LINE_REFRESH_DOWN;
    pos->line = line;
    pos->offset = 0;

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

