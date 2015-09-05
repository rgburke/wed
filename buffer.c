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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "status.h"
#include "display.h"
#include "file.h"
#include "config.h"
#include "encoding.h"

#define FILE_BUF_SIZE 1024
#define DETECT_FF_LINE_NUM 5

static Status reset_buffer(Buffer *);
static int is_selection(Direction *);
static void bf_default_movement_selection_handler(Buffer *, int, Direction *);
static Status bf_change_real_line(Buffer *, BufferPos *, Direction, int);
static Status bf_change_screen_line(Buffer *, BufferPos *, Direction, int);
static Status bf_advance_bp_to_line_offset(Buffer *, BufferPos *, int);
static void bf_update_line_col_offset(Buffer *, BufferPos *);
static Status bf_insert_expanded_tab(Buffer *, int);
static Status bf_auto_indent(Buffer *, int);
static Status bf_convert_fileformat(TextSelection *, TextSelection *, int *);

Buffer *bf_new(const FileInfo *file_info, const HashMap *config)
{
    assert(file_info != NULL);

    Buffer *buffer = malloc(sizeof(Buffer));
    RETURN_IF_NULL(buffer);

    memset(buffer, 0, sizeof(Buffer));

    if ((buffer->config = new_hashmap()) == NULL) {
        bf_free(buffer);
        return NULL;
    }

    if ((buffer->data = gb_new(GAP_INCREMENT)) == NULL) {
        bf_free(buffer);
        return NULL;
    }

    if (!cf_populate_config(config, buffer->config, CL_BUFFER)) {
        bf_free(buffer);
        return NULL;
    }

    buffer->file_info = *file_info;
    buffer->encoding_type = ENC_UTF8;
    buffer->file_format = FF_UNIX;
    en_init_char_enc_funcs(buffer->encoding_type, &buffer->cef);
    bp_init(&buffer->pos, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bp_init(&buffer->screen_start, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bp_init(&buffer->select_start, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bf_select_reset(buffer);
    init_window_info(&buffer->win_info);
    bs_init_default_opt(&buffer->search);
    bc_init(&buffer->changes);

    return buffer;
}

Buffer *bf_new_empty(const char *file_name, const HashMap *config)
{
    FileInfo file_info;

    if (!fi_init_empty(&file_info, file_name)) {
        return NULL;
    }

    Buffer *buffer = bf_new(&file_info, config); 
    RETURN_IF_NULL(buffer);

    return buffer;
}

void bf_free(Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    bs_free(&buffer->search);
    fi_free(&buffer->file_info);
    cf_free_config(buffer->config);
    gb_free(buffer->data);
    bc_free(&buffer->changes);

    free(buffer);
}

Status bf_clear(Buffer *buffer)
{
    GapBuffer *data = buffer->data;

    RETURN_IF_FAIL(reset_buffer(buffer));

    gb_free(data);

    return STATUS_SUCCESS;
}

static Status reset_buffer(Buffer *buffer)
{
    buffer->data = gb_new(GAP_INCREMENT);

    if (buffer->data == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to reset buffer");
    }

    bp_init(&buffer->pos, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bp_init(&buffer->screen_start, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bp_init(&buffer->select_start, buffer->data, &buffer->cef, &buffer->file_format, buffer->config);
    bf_select_reset(buffer);
    bf_update_line_col_offset(buffer, &buffer->pos);

    return STATUS_SUCCESS;
}

FileFormat bf_detect_fileformat(const Buffer *buffer)
{
    FileFormat file_format = FF_UNIX;

    if (gb_lines(buffer->data) > 0) {
        size_t lines = MIN(gb_lines(buffer->data), DETECT_FF_LINE_NUM);
        size_t unix_le = 0;
        size_t dos_le = 0;
        size_t point = 0;

        do {
            if (!gb_find_next(buffer->data, point, &point, '\n')) {
                break;
            }

            if (point > 0 && gb_get_at(buffer->data, point - 1) == '\r') {
                dos_le++; 
            } else {
                unix_le++;
            }
        } while (--lines != 0);

        if (dos_le > unix_le) {
            file_format = FF_WINDOWS;
        }
    }

    return file_format;
}

/* Loads file into buffer structure */
Status bf_load_file(Buffer *buffer)
{
    Status status = STATUS_SUCCESS;

    if (!fi_file_exists(&buffer->file_info)) {
        /* If the file represented by this buffer doesn't exist
         * then the buffer content is empty */
        return reset_buffer(buffer);
    }

    FILE *input_file = fopen(buffer->file_info.rel_path, "rb");

    if (input_file == NULL) {
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE, "Unable to open file %s for reading - %s", 
                         buffer->file_info.file_name, strerror(errno));
    } 

    gb_set_point(buffer->data, 0);
    gb_preallocate(buffer->data, buffer->file_info.file_stat.st_size);

    char buf[FILE_BUF_SIZE];
    size_t read;

    do {
        read = fread(buf, sizeof(char), FILE_BUF_SIZE, input_file);

        if (ferror(input_file)) {
            status = st_get_error(ERR_UNABLE_TO_READ_FILE, "Unable to read from file %s - %s", 
                               buffer->file_info.file_name, strerror(errno));
            break;
        } 

        if (!gb_add(buffer->data, buf, read)) {
            status = st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to populate buffer");
            break;
        }
    } while (read == FILE_BUF_SIZE);

    gb_set_point(buffer->data, 0);

    fclose(input_file);

    return status;
}

/* Used when loading a file into a buffer */
Status bf_write_file(Buffer *buffer, const char *file_path)
{
    assert(!is_null_or_empty(file_path));

    const FileInfo *file_info = &buffer->file_info;

    size_t tmp_file_path_len = strlen(file_path) + 6 + 1;
    char *tmp_file_path = malloc(tmp_file_path_len);

    if (tmp_file_path == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to create temporary file path");
    }

    snprintf(tmp_file_path, tmp_file_path_len, "%sXXXXXX", file_path);

    int output_file = mkstemp(tmp_file_path);

    if (output_file == -1) {
        free(tmp_file_path);
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE, "Unable to open file temporary for writing - %s",
                         strerror(errno));
    }

    Status status = STATUS_SUCCESS;
    size_t buffer_len = gb_length(buffer->data);
    size_t bytes_remaining = buffer_len;
    size_t bytes_retrieved;
    char buf[FILE_BUF_SIZE];

    while (bytes_remaining > 0) {
        bytes_retrieved = gb_get_range(buffer->data, buffer_len - bytes_remaining, buf, FILE_BUF_SIZE);

        if (write(output_file, buf, bytes_retrieved) <= 0) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to write to temporary file - %s", 
                    strerror(errno));
            break;
        }

        bytes_remaining -= bytes_retrieved;
    }

    close(output_file);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    struct stat file_stat;

    if (stat(file_path, &file_stat) == 0) {
        if (chmod(tmp_file_path, file_stat.st_mode) == -1) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to set file permissions - %s", 
                               strerror(errno));
            goto cleanup;
        }

        if (chown(tmp_file_path, file_stat.st_uid, file_stat.st_gid) == -1) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to set owner - %s",
                               strerror(errno));
            goto cleanup;
        }
    }

    if (rename(tmp_file_path, file_path) == -1) {
        status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE, "Unable to overwrite file %s - %s",
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

char *bf_to_string(const Buffer *buffer)
{
    size_t buffer_len = gb_length(buffer->data);

    char *str = malloc(buffer_len + 1);

    if (str == NULL) {
        return NULL;
    }

    gb_get_range(buffer->data, 0, str, buffer_len);
    *(str + buffer_len) = '\0';

    return str;
}

char *bf_join_lines(const Buffer *buffer, const char *seperator)
{
    assert(seperator != NULL);

    char *buffer_str = bf_to_string(buffer);

    if (buffer_str == NULL) {
        return NULL;
    }

    const char *new_line = bf_new_line_str(buffer->file_format);
    char *joined = replace(buffer_str, new_line, seperator);

    free(buffer_str);

    return joined;
}

int bf_is_empty(const Buffer *buffer)
{
    return gb_length(buffer->data) == 0;
}

size_t bf_lines(const Buffer *buffer)
{
    return gb_lines(buffer->data) + 1;
}

size_t bf_length(const Buffer *buffer)
{
    return gb_length(buffer->data);
}

int bf_get_range(Buffer *buffer, Range *range)
{
    if (!bf_selection_started(buffer)) {
        return 0;
    } else if (bp_compare(&buffer->pos, &buffer->select_start) == 0) {
        bf_select_reset(buffer);
        return 0;
    }

    range->start = bp_min(&buffer->pos, &buffer->select_start);
    range->end = bp_max(&buffer->pos, &buffer->select_start);

    return 1;
}

int bf_bp_in_range(const Range *range, const BufferPos *pos)
{
    if (bp_compare(pos, &range->start) < 0 || bp_compare(pos, &range->end) >= 0) {
        return 0;
    }

    return 1;
}

/* TODO Consider UTF-8 punctuation and whitespace */
CharacterClass bf_character_class(const Buffer *buffer, const BufferPos *pos)
{
    CharInfo char_info;
    pos->cef->char_info(&char_info, CIP_DEFAULT, *pos, buffer->config);

    if (char_info.byte_length == 1) {
        uchar character = bp_get_uchar(pos);

        if (isspace(character)) {
            return CCLASS_WHITESPACE;
        } else if (ispunct(character)) {
            return CCLASS_PUNCTUATION;
        }
    }

    return CCLASS_WORD;
}

int bf_get_fileformat(const char *ff_name, FileFormat *file_format)
{
    assert(!is_null_or_empty(ff_name));

    if (strncmp(ff_name, "unix", 5) == 0) {
        *file_format = FF_UNIX;
        return 1;
    } else if (strncmp(ff_name, "windows", 8) == 0 ||
               strncmp(ff_name, "dos", 4) == 0) {
        *file_format = FF_WINDOWS;
        return 1;
    } 

    return 0;
}

const char *bf_get_fileformat_str(FileFormat file_format)
{
    if (file_format == FF_UNIX) {
        return "unix";
    } else if (file_format == FF_WINDOWS) {
        return "windows";
    }

    assert(!"Invalid FileFormat");

    return "unix";
}

void bf_set_fileformat(Buffer *buffer, FileFormat file_format)
{
    assert(file_format == FF_UNIX || file_format == FF_WINDOWS);
    buffer->file_format = file_format;
}

const char *bf_new_line_str(FileFormat file_format)
{
    if (file_format == FF_UNIX) {
        return "\n";
    } else if (file_format == FF_WINDOWS) {
        return "\r\n";
    }

    assert(!"Invalid FileFormat");

    return "\n";
}

int bf_bp_at_screen_line_start(const Buffer *buffer, const BufferPos *pos)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        size_t screen_col_no = (pos->col_no - 1) % buffer->win_info.width;

        if (screen_col_no == 0) {
            return 1;
        }

        BufferPos prev_char = *pos;
        bp_prev_char(&prev_char);

        size_t prev_screen_col_no = (prev_char.col_no - 1) % buffer->win_info.width;

        if (prev_screen_col_no == 0) {
            return 0;
        }

        return screen_col_no < prev_screen_col_no;
    }

    return bp_at_line_start(pos);
}

int bf_bp_at_screen_line_end(const Buffer *buffer, const BufferPos *pos)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        size_t screen_col_no = pos->col_no % buffer->win_info.width; 

        if (screen_col_no == 0) {
            return 1;
        }

        BufferPos next_char = *pos;
        bp_next_char(&next_char);

        size_t next_screen_col_no = next_char.col_no % buffer->win_info.width;

        if (next_screen_col_no == 0) {
            return 0;
        }

        return screen_col_no > next_screen_col_no;
    }

    return bp_at_line_end(pos);
}

int bf_bp_move_past_buffer_extremes(const BufferPos *pos, Direction direction)
{
    return ((direction == DIRECTION_LEFT && bp_at_buffer_start(pos)) ||
            (direction == DIRECTION_RIGHT && bp_at_buffer_end(pos)));
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

int bf_selection_started(const Buffer *buffer)
{
    return buffer->select_start.line_no > 0;
}

static void bf_default_movement_selection_handler(Buffer *buffer, int is_select, Direction *direction)
{
    if (is_select) {
        if (direction != NULL) {
            *direction |= DIRECTION_WITH_SELECT;
        }

        bf_select_continue(buffer);
    } else if (bf_selection_started(buffer)) {
        bf_select_reset(buffer);
    }
}

Status bf_set_bp(Buffer *buffer, const BufferPos *pos)
{
    assert(pos->data == buffer->data);
    assert(pos->offset <= gb_length(pos->data));

    if (pos->data != buffer->data ||
        pos->offset > gb_length(pos->data)) {
        return st_get_error(ERR_INVALID_BUFFERPOS, "Invalid Buffer Position");
    }

    buffer->pos = *pos;

    if (bf_selection_started(buffer)) {
        bf_select_reset(buffer);
    }

    return STATUS_SUCCESS;
}

/* Move cursor up or down a line keeping the offset into the line the same 
 * or as close to the original if possible */
Status bf_change_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        return bf_change_screen_line(buffer, pos, direction, is_cursor);
    }

    return bf_change_real_line(buffer, pos, direction, is_cursor);
}

static Status bf_change_real_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    assert(direction == DIRECTION_UP || direction == DIRECTION_DOWN);

    if (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN)) {
        return STATUS_SUCCESS;
    }

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select, NULL);
    }

    if ((direction == DIRECTION_DOWN && bp_at_last_line(pos)) ||
        (direction == DIRECTION_UP && bp_at_first_line(pos))) {
        return STATUS_SUCCESS;
    }

    if (direction == DIRECTION_DOWN) {
        bp_next_line(pos);
    } else {
        bp_prev_line(pos);
    }

    if (is_cursor) {
        return bf_advance_bp_to_line_offset(buffer, pos, is_select);
    }

    return STATUS_SUCCESS;
}

/* Move cursor up or down a screen line keeping the cursor column as close to the
 * starting value as possible. For lines which don't wrap this function behaves the
 * same as bf_change_line. For lines which wrap this allows a user to scroll up or
 * down to a different part of the line displayed as a different line on the screen.
 * Therefore this function is dependent on the width of the screen. */

static Status bf_change_screen_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    assert(direction == DIRECTION_UP || direction == DIRECTION_DOWN);

    if (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN)) {
        return STATUS_SUCCESS;
    }

    Direction pos_direction = (direction == DIRECTION_DOWN ? DIRECTION_RIGHT : DIRECTION_LEFT);

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select, &pos_direction);
    }

    size_t start_col = screen_col_no(buffer, pos);

    if (direction == DIRECTION_UP) {
        if (!bf_bp_at_screen_line_start(buffer, pos)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_start(buffer, pos, is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (!bf_bp_at_screen_line_start(buffer, pos) &&
               screen_col_no(buffer, pos) > start_col) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));
        }
    } else {
        if (!bf_bp_at_screen_line_end(buffer, pos)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_end(buffer, pos, is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (!bp_at_line_end(pos) &&
               !bf_bp_at_screen_line_end(buffer, pos) &&
               screen_col_no(buffer, pos) < start_col) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));
        }
    }

    if (is_cursor) {
        RETURN_IF_FAIL(bf_advance_bp_to_line_offset(buffer, pos, is_select));
    }

    return STATUS_SUCCESS;
}

static Status bf_advance_bp_to_line_offset(Buffer *buffer, BufferPos *pos, int is_select)
{
    size_t global_col_offset = buffer->line_col_offset;
    size_t current_col_offset = screen_col_no(buffer, pos) - 1;
    Direction direction = DIRECTION_RIGHT;

    if (is_select) {
        direction |= DIRECTION_WITH_SELECT;
    }

    size_t last_col;

    while (!bp_at_line_end(pos) && current_col_offset < global_col_offset) {
        last_col = pos->col_no;
        RETURN_IF_FAIL(bf_change_char(buffer, pos, direction, 1));
        current_col_offset += pos->col_no - last_col;
    }

    return STATUS_SUCCESS;
}

Status bf_change_multi_line(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset, int is_cursor)
{
    if (offset == 0) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = bf_change_line(buffer, pos, direction, is_cursor);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

/* Move cursor a character to the left or right */
Status bf_change_char(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    assert(direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT);

    if (!(direction == DIRECTION_LEFT || direction == DIRECTION_RIGHT)) {
        return STATUS_SUCCESS;
    }

    if (is_cursor) {
        if (is_select) {
            if (!bf_bp_move_past_buffer_extremes(pos, direction)) {
                bf_select_continue(buffer);
            }
        } else if (bf_selection_started(buffer)) {
            Range select_range;
            BufferPos new_pos;

            bf_get_range(buffer, &select_range);

            if (direction == DIRECTION_LEFT) {
                new_pos = select_range.start;
            } else {
                new_pos = select_range.end;
            }

            bf_select_reset(buffer);
            buffer->pos = new_pos;
            bf_update_line_col_offset(buffer, &buffer->pos);
            return STATUS_SUCCESS;
        }
    }

    if (bf_bp_move_past_buffer_extremes(pos, direction)) {
        return STATUS_SUCCESS;
    }

    if (direction == DIRECTION_LEFT) {
        bp_prev_char(pos);
    } else {
        bp_next_char(pos);
    }

    if (is_cursor) {
        bf_update_line_col_offset(buffer, pos);
    }

    return STATUS_SUCCESS;
}

Status bf_change_multi_char(Buffer *buffer, BufferPos *pos, Direction direction, size_t offset, int is_cursor)
{
    if (offset == 0) {
        return STATUS_SUCCESS;
    }

    Status status;

    for (size_t k = 0; k < offset; k++) {
        status = bf_change_char(buffer, pos, direction, is_cursor);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

static void bf_update_line_col_offset(Buffer *buffer, BufferPos *pos)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        /* Windowinfo may not be initialised when the error buffer is populated,
         * but line_col_offset isn't needed in this case anyway. */
        if (buffer->win_info.width > 0) {
            buffer->line_col_offset = (pos->col_no - 1) % buffer->win_info.width;
        }
    } else {
        buffer->line_col_offset = pos->col_no - 1;
    }
}

Status bf_to_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        return bf_bp_to_screen_line_start(buffer, pos, is_select, is_cursor);
    }

    return bf_bp_to_line_start(buffer, pos, is_select, is_cursor);
}

Status bf_bp_to_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    if (is_cursor) {
        Direction direction = DIRECTION_LEFT;
        bf_default_movement_selection_handler(buffer, is_select, &direction);
    }

    bp_to_line_start(pos);

    if (is_cursor) {
        bf_update_line_col_offset(buffer, &buffer->pos);
    }

    return STATUS_SUCCESS;
}

Status bf_bp_to_screen_line_start(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    Direction direction = DIRECTION_LEFT;

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select, &direction);
    }

    if (bp_at_line_start(pos)) {
        return STATUS_SUCCESS;
    }

    do {
        RETURN_IF_FAIL(bf_change_char(buffer, pos, direction, is_cursor));
    } while (pos->offset > 0 && !bf_bp_at_screen_line_start(buffer, pos));

    return STATUS_SUCCESS;
}

Status bf_to_line_end(Buffer *buffer, int is_select)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        return bf_bp_to_screen_line_end(buffer, &buffer->pos, is_select, 1);
    }

    return bf_bp_to_line_end(buffer, &buffer->pos, is_select, 1);
}

Status bf_bp_to_line_end(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    Direction direction = DIRECTION_RIGHT;

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select, &direction);
    }

    bp_to_line_end(pos);

    if (is_cursor) {
        bf_update_line_col_offset(buffer, pos);
    }

    return STATUS_SUCCESS;
}

Status bf_bp_to_screen_line_end(Buffer *buffer, BufferPos *pos, int is_select, int is_cursor)
{
    Direction direction = DIRECTION_RIGHT;

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select, &direction);
    }

    if (bp_at_line_end(pos)) {
        return STATUS_SUCCESS;
    }

    do {
        RETURN_IF_FAIL(bf_change_char(buffer, pos, direction, is_cursor));
    } while (!bp_at_line_end(pos) &&
             !bf_bp_at_screen_line_end(buffer, pos));

    return STATUS_SUCCESS;
}

Status bf_to_next_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    bf_default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    if (is_select) {
        while (bf_character_class(buffer, pos) == CCLASS_WHITESPACE) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, direction, 1));

            if (bp_at_line_start(pos) || bp_at_buffer_end(pos)) {
                return STATUS_SUCCESS;
            }
        }
    }

    CharacterClass start_class = bf_character_class(buffer, pos);

    do {
        status = bf_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    } while (!bp_at_buffer_end(pos) &&
             start_class == bf_character_class(buffer, pos));

    if (is_select) {
        return STATUS_SUCCESS;
    }

    while (!bp_at_line_end(pos) &&
           bf_character_class(buffer, pos) == CCLASS_WHITESPACE) {

        status = bf_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

Status bf_to_prev_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_LEFT;
    bf_default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    do {
        status = bf_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);

        if (bp_at_line_end(pos) || bp_at_buffer_start(pos)) {
            return STATUS_SUCCESS;
        }
    } while (bf_character_class(buffer, pos) == CCLASS_WHITESPACE);

    CharacterClass start_class = bf_character_class(buffer, pos);
    BufferPos look_ahead = buffer->pos;

    while (!bp_at_line_start(pos)) {
        RETURN_IF_FAIL(bf_change_char(buffer, &look_ahead, DIRECTION_LEFT, 0));

        if (start_class != bf_character_class(buffer, &look_ahead)) {
            break;
        }

        status = bf_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    }

    return STATUS_SUCCESS;
}

Status bf_to_buffer_start(Buffer *buffer, int is_select)
{
    bf_default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;

    bf_update_line_col_offset(buffer, pos);

    return STATUS_SUCCESS;
}

Status bf_to_buffer_end(Buffer *buffer, int is_select)
{
    bf_default_movement_selection_handler(buffer, is_select, NULL);

    BufferPos *pos = &buffer->pos;
    pos->offset = gb_length(pos->data);
    pos->line_no = gb_lines(pos->data) + 1;
    pos->col_no = 1;
    bp_recalc_col(pos);

    return STATUS_SUCCESS;
}

Status bf_change_page(Buffer *buffer, Direction direction)
{
    int is_select = is_selection(&direction);
    BufferPos *pos = &buffer->pos;

    if (bp_at_first_line(pos) && direction == DIRECTION_UP) {
        return STATUS_SUCCESS;
    }

    bf_default_movement_selection_handler(buffer, is_select, &direction);

    Status status = bf_change_multi_line(buffer, pos, direction, buffer->win_info.height - 1, 1);

    RETURN_IF_FAIL(status);

    if (buffer->screen_start.line_no != buffer->pos.line_no) {
        buffer->screen_start = buffer->pos;
        RETURN_IF_FAIL(bf_bp_to_screen_line_start(buffer, &buffer->screen_start, 0, 0));
    }

    return STATUS_SUCCESS;
}

static Status bf_insert_expanded_tab(Buffer *buffer, int advance_cursor)
{
    static char spaces[CFG_TABWIDTH_MAX + 1];
    size_t tabwidth = cf_int(buffer->config, CV_TABWIDTH);
    tabwidth = tabwidth - ((buffer->pos.col_no - 1) % tabwidth);
    memset(spaces, ' ', tabwidth);
    spaces[tabwidth] = '\0';

    return bf_insert_string(buffer, spaces, tabwidth, advance_cursor);
}

static Status bf_auto_indent(Buffer *buffer, int advance_cursor)
{
    BufferPos tmp = buffer->pos;
    bp_to_line_start(&tmp);
    size_t line_start_offset = tmp.offset;

    while (!bp_at_line_end(&tmp) &&
           bf_character_class(buffer, &tmp) == CCLASS_WHITESPACE) {
        bp_next_char(&tmp);                  
    }

    const char *new_line = bf_new_line_str(buffer->file_format);
    size_t new_line_len = strlen(new_line);

    if (!(tmp.offset > line_start_offset)) {
        return bf_insert_string(buffer, new_line, new_line_len, advance_cursor);
    }

    size_t indent_length = tmp.offset - line_start_offset;
    char *indent = malloc(indent_length + new_line_len);

    if (indent == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to insert character");
    }

    gb_get_range(buffer->data, line_start_offset, indent + new_line_len, indent_length);
    memcpy(indent, new_line, new_line_len);

    Status status = bf_insert_string(buffer, indent, indent_length + new_line_len, advance_cursor);
    free(indent);

    return status;
}

Status bf_insert_character(Buffer *buffer, const char *character, int advance_cursor)
{
    size_t char_len = 0;
    
    if (character != NULL) {
        char_len = strnlen(character, 5);
    }

    if (char_len == 0 || char_len > 4) {
        return st_get_error(ERR_INVALID_CHARACTER, "Invalid character %s", character);
    }

    if (*character == '\t' && cf_bool(buffer->config, CV_EXPANDTAB)) {
        return bf_insert_expanded_tab(buffer, advance_cursor);
    } else if (*character == '\n' && cf_bool(buffer->config, CV_AUTOINDENT)) {
        return bf_auto_indent(buffer, advance_cursor);
    }

    return bf_insert_string(buffer, character, char_len, advance_cursor);
}

Status bf_insert_string(Buffer *buffer, const char *string, size_t string_length, int advance_cursor)
{
    if (string == NULL) {
        return st_get_error(ERR_INVALID_CHARACTER, "Cannot insert NULL string");
    } else if (string_length == 0) {
        return STATUS_SUCCESS;
    }

    Status status = STATUS_SUCCESS;

    Range range;
    int grouped_changes_started = 0;

    if (bf_get_range(buffer, &range)) {
        if (!bc_grouped_changes_started(&buffer->changes)) {
            RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));
            grouped_changes_started = 1;
        }

        status = bf_delete_range(buffer, &range);
        
        if (!STATUS_IS_SUCCESS(status)) {
            goto cleanup;
        }
    }

    BufferPos *pos = &buffer->pos;
    BufferPos start_pos = *pos;
    gb_set_point(buffer->data, pos->offset);
    size_t lines_before;

    if (advance_cursor) {
        lines_before = gb_lines(pos->data);
    }

    if (!gb_insert(buffer->data, string, string_length)) {
        status = st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert text");
        goto cleanup;
    }

    buffer->is_dirty = 1;

    status = bc_add_text_insert(&buffer->changes, string_length, &start_pos);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    if (advance_cursor) {
        /* TODO Somewhat arbitrary */
        if (string_length > 100) {
            size_t lines_after = gb_lines(buffer->data);
            pos->line_no += lines_after - lines_before;
            pos->offset += string_length;
            bp_recalc_col(pos);
            bf_update_line_col_offset(buffer, pos);
        } else {
            size_t end_offset = pos->offset + string_length;

            while (STATUS_IS_SUCCESS(status) && pos->offset < end_offset) {
                status = bf_change_char(buffer, pos, DIRECTION_RIGHT, 1);
            }
        }
    }

cleanup:
    if (grouped_changes_started) {
        bc_end_grouped_changes(&buffer->changes);
    }

    return status;
} 

Status bf_replace_string(Buffer *buffer, size_t replace_length, const char *string, 
                         size_t string_length, int advance_cursor)
{
    if (string == NULL) {
        return st_get_error(ERR_INVALID_CHARACTER, "Cannot insert NULL string");
    }

    int grouped_changes_started = bc_grouped_changes_started(&buffer->changes);

    if (!grouped_changes_started) {
        RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));
    }

    Status status = bf_delete(buffer, replace_length);

    if (STATUS_IS_SUCCESS(status)) {
        buffer->is_dirty = 1;
        status = bf_insert_string(buffer, string, string_length, 1);
    }

    if (!grouped_changes_started) {
        status = bc_end_grouped_changes(&buffer->changes);
    }

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    if (advance_cursor) {
        bp_advance_to_offset(&buffer->pos, gb_get_point(buffer->data));
    }

    return status;
}

Status bf_delete(Buffer *buffer, size_t byte_num)
{
    Range range;

    if (bf_get_range(buffer, &range)) {
        return bf_delete_range(buffer, &range);
    }

    BufferPos *pos = &buffer->pos;

    if (bp_at_buffer_end(pos) || byte_num == 0) {
        return STATUS_SUCCESS;
    }

    assert(gb_length(buffer->data) - pos->offset >= byte_num);

    char *deleted_str = malloc(byte_num);

    if (deleted_str == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - "
                            "Unable to save deletion");
    }

    gb_get_range(buffer->data, pos->offset, deleted_str, byte_num);

    gb_set_point(buffer->data, pos->offset);

    if (!gb_delete(buffer->data, byte_num)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - "
                            "Unable to delete character");
    }

    buffer->is_dirty = 1;

    Status status = bc_add_text_delete(&buffer->changes, deleted_str,
                                       byte_num, pos); 

    free(deleted_str);

    return status;
}

Status bf_delete_character(Buffer *buffer)
{
    size_t byte_length;

    if (bp_at_line_end(&buffer->pos) &&
        buffer->file_format == FF_WINDOWS &&
        bp_get_char(&buffer->pos) == '\r') {
        byte_length = 2;
    } else {
        CharInfo char_info;
        buffer->cef.char_info(&char_info, CIP_DEFAULT, 
                              buffer->pos, buffer->config);
        byte_length = char_info.byte_length;
    }

    return bf_delete(buffer, byte_length);
}

Status bf_select_continue(Buffer *buffer)
{
    if (buffer->select_start.line_no == 0) {
        buffer->select_start = buffer->pos;
    }

    return STATUS_SUCCESS;
}

Status bf_select_reset(Buffer *buffer)
{
    buffer->select_start.line_no = 0;
    return STATUS_SUCCESS;
}

Status bf_delete_range(Buffer *buffer, const Range *range)
{
    bf_select_reset(buffer);
    BufferPos *pos = &buffer->pos;
    *pos = range->start;

    size_t delete_byte_num = range->end.offset - range->start.offset;
    assert(delete_byte_num > 0);

    return bf_delete(buffer, delete_byte_num);
}

Status bf_select_all_text(Buffer *buffer)
{
    if (gb_length(buffer->data) == 0) {
        return STATUS_SUCCESS;
    }

    BufferPos *pos = &buffer->pos, *select_start = &buffer->select_start;

    select_start->line_no = gb_lines(buffer->data) + 1;
    select_start->offset = gb_length(buffer->data);
    bp_recalc_col(select_start);

    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;

    return STATUS_SUCCESS;
}

Status bf_copy_selected_text(Buffer *buffer, TextSelection *text_selection)
{
    memset(text_selection, 0, sizeof(TextSelection));
    Range range;

    if (!bf_get_range(buffer, &range)) {
        return STATUS_SUCCESS;
    }

    assert(range.end.offset > range.start.offset);
    size_t range_size = range.end.offset - range.start.offset;

    text_selection->str = malloc(range_size + 1);

    if (text_selection->str == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy selected text");
    }

    size_t copied = gb_get_range(buffer->data, range.start.offset, text_selection->str, range_size);

    (void)copied;
    assert(copied == range_size);

    *(text_selection->str + range_size) = '\0';
    text_selection->str_len = range_size;
    text_selection->file_format = buffer->file_format;

    return STATUS_SUCCESS;
}

Status bf_cut_selected_text(Buffer *buffer, TextSelection *text_selection)
{
    memset(text_selection, 0, sizeof(TextSelection));
    Range range;

    if (!bf_get_range(buffer, &range)) {
        return STATUS_SUCCESS;
    }
    
    Status status = bf_copy_selected_text(buffer, text_selection);

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    return bf_delete_range(buffer, &range);
}

Status bf_insert_textselection(Buffer *buffer, TextSelection *text_selection)
{
    assert(text_selection->str != NULL);
    
    if (text_selection->str_len == 0) {
        return STATUS_SUCCESS;
    }

    if (text_selection->file_format != buffer->file_format) {
        TextSelection converted_selection = { .file_format = buffer->file_format };
        int conversion_perfomed;

        Status status = bf_convert_fileformat(text_selection, &converted_selection,
                                              &conversion_perfomed);
        RETURN_IF_FAIL(status);

        if (conversion_perfomed) {
            status = bf_insert_string(buffer, converted_selection.str, 
                                      converted_selection.str_len, 1);

            bf_free_textselection(&converted_selection);
            return status;
        }
    }

    return bf_insert_string(buffer, text_selection->str, text_selection->str_len, 1);
}

static Status bf_convert_fileformat(TextSelection *in_ts, TextSelection *out_ts,
                                    int *conversion_perfomed)
{
    *conversion_perfomed = 0;

    if (in_ts->file_format == out_ts->file_format ||
        memchr(in_ts->str, '\n', in_ts->str_len) == NULL) {
        *out_ts = *in_ts;
        return STATUS_SUCCESS;
    }

    out_ts->str = NULL;

    if (in_ts->file_format == FF_UNIX && 
        out_ts->file_format == FF_WINDOWS) {
        out_ts->str = replace(in_ts->str, "\n", "\r\n");
    } else if (in_ts->file_format == FF_WINDOWS && 
               out_ts->file_format == FF_UNIX) {
        out_ts->str = replace(in_ts->str, "\r\n", "\n");
    } else {
        assert(!"Unsupported file format");
    }

    if (out_ts->str == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to convert fileformat of text");
    }

    out_ts->str_len = strlen(out_ts->str);
    *conversion_perfomed = 1;

    return STATUS_SUCCESS;
}

void bf_free_textselection(TextSelection *text_selection)
{
    if (text_selection == NULL) {
        return;
    }

    free(text_selection->str);
    memset(text_selection, 0, sizeof(TextSelection));
}

Status bf_delete_word(Buffer *buffer)
{
    Range range;

    if (bf_get_range(buffer, &range)) {
        return bf_delete_range(buffer, &range);
    }

    if (bp_at_buffer_end(&buffer->pos)) {
        return STATUS_SUCCESS;
    }

    BufferPos select_start = buffer->pos;
    RETURN_IF_FAIL(bf_to_next_word(buffer, 0));
    buffer->select_start = select_start;

    bf_get_range(buffer, &range);
    RETURN_IF_FAIL(bf_delete_range(buffer, &range));

    return STATUS_SUCCESS;
}

Status bf_delete_prev_word(Buffer *buffer)
{
    Range range;

    if (bf_get_range(buffer, &range)) {
        return bf_delete_range(buffer, &range);
    }

    if (bp_at_buffer_start(&buffer->pos)) {
        return STATUS_SUCCESS;
    }

    BufferPos select_start = buffer->pos;
    RETURN_IF_FAIL(bf_to_prev_word(buffer, 0));
    buffer->select_start = select_start;

    bf_get_range(buffer, &range);
    RETURN_IF_FAIL(bf_delete_range(buffer, &range));

    return STATUS_SUCCESS;
}

Status bf_set_text(Buffer *buffer, const char *text)
{
    RETURN_IF_FAIL(bf_clear(buffer));

    if (text != NULL) {
        return bf_insert_string(buffer, text, strlen(text), 1);
    }

    return STATUS_SUCCESS;
}

