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

static Status reset_buffer(Buffer *);
static int is_selection(Direction *);
static void bf_default_movement_selection_handler(Buffer *, int, Direction *);
static Status bf_change_real_line(Buffer *, BufferPos *, Direction, int);
static Status bf_change_screen_line(Buffer *, BufferPos *, Direction, int);
static Status bf_advance_bp_to_line_offset(Buffer *, BufferPos *, int);
static void bf_update_line_col_offset(Buffer *, BufferPos *);

Buffer *bf_new(const FileInfo *file_info)
{
    assert(file_info != NULL);

    Buffer *buffer = malloc(sizeof(Buffer));
    RETURN_IF_NULL(buffer);

    memset(buffer, 0, sizeof(Buffer));

    if ((buffer->config = new_hashmap()) == NULL) {
        free(buffer);
        return NULL;
    }

    if ((buffer->data = gb_new(GAP_INCREMENT)) == NULL) {
        cf_free_config(buffer->config);
        free(buffer);
        return NULL;
    }

    buffer->file_info = *file_info;
    buffer->encoding_type = ENC_UTF8;
    en_init_char_enc_funcs(buffer->encoding_type, &buffer->cef);
    bp_init(&buffer->pos, buffer->data, &buffer->cef);
    bp_init(&buffer->screen_start, buffer->data, &buffer->cef);
    bp_init(&buffer->select_start, buffer->data, &buffer->cef);
    bf_select_reset(buffer);
    init_window_info(&buffer->win_info);

    return buffer;
}

Buffer *bf_new_empty(const char *file_name)
{
    FileInfo file_info;

    if (!fi_init_empty(&file_info, file_name)) {
        return NULL;
    }

    Buffer *buffer = bf_new(&file_info); 
    RETURN_IF_NULL(buffer);

    return buffer;
}

void bf_free(Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    fi_free(&buffer->file_info);
    cf_free_config(buffer->config);
    gb_free(buffer->data);

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

    bp_init(&buffer->pos, buffer->data, &buffer->cef);
    bp_init(&buffer->screen_start, buffer->data, &buffer->cef);
    bf_update_line_col_offset(buffer, &buffer->pos);
    bf_select_reset(buffer);

    return STATUS_SUCCESS;
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

    char *joined = replace(buffer_str, "\n", seperator);

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

int bf_get_range(const Buffer *buffer, Range *range)
{
    if (!bf_selection_started(buffer)) {
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
CharacterClass bf_character_class(const BufferPos *pos)
{
    CharInfo char_info;
    pos->cef->char_info(&char_info, CIP_DEFAULT, *pos);

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

int bf_bp_at_screen_line_start(const BufferPos *pos, const WindowInfo *win_info)
{
    if (cf_bool("linewrap")) {
        return ((pos->col_no - 1) % win_info->width) == 0;
    }

    return bp_at_line_start(pos);
}

int bf_bp_at_screen_line_end(const BufferPos *pos, const WindowInfo *win_info)
{
    if (cf_bool("linewrap")) {
        return (pos->col_no % win_info->width) == 0;
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

/* Move cursor up or down a line keeping the offset into the line the same 
 * or as close to the original if possible */
Status bf_change_line(Buffer *buffer, BufferPos *pos, Direction direction, int is_cursor)
{
    if (cf_bool("linewrap")) {
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

    size_t start_col = screen_col_no(buffer->win_info, *pos);

    if (direction == DIRECTION_UP) {
        if (!bf_bp_at_screen_line_start(pos, &buffer->win_info)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_start(buffer, pos, is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (screen_col_no(buffer->win_info, *pos) > start_col) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));
        }
    } else {
        if (!bf_bp_at_screen_line_end(pos, &buffer->win_info)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_end(buffer, pos, is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (!bp_at_line_end(pos) && 
                screen_col_no(buffer->win_info, *pos) < start_col) {
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
    size_t current_col_offset = screen_col_no(buffer->win_info, *pos) - 1;
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
    if (cf_bool("linewrap")) {
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
    if (cf_bool("linewrap")) {
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
    } while (pos->offset > 0 && !bf_bp_at_screen_line_start(pos, &buffer->win_info));

    return STATUS_SUCCESS;
}

Status bf_to_line_end(Buffer *buffer, int is_select)
{
    if (cf_bool("linewrap")) {
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
             !bf_bp_at_screen_line_end(pos, &buffer->win_info));

    return STATUS_SUCCESS;
}

Status bf_to_next_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    bf_default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    if (is_select) {
        while (bf_character_class(pos) == CCLASS_WHITESPACE) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, direction, 1));

            if (bp_at_line_start(pos) || bp_at_buffer_end(pos)) {
                return STATUS_SUCCESS;
            }
        }
    }

    CharacterClass start_class = bf_character_class(pos);

    do {
        status = bf_change_char(buffer, pos, direction, 1);
        RETURN_IF_FAIL(status);
    } while (!bp_at_buffer_end(pos) &&
             start_class == bf_character_class(pos));

    if (is_select) {
        return STATUS_SUCCESS;
    }

    while (!bp_at_line_end(pos) &&
           bf_character_class(pos) == CCLASS_WHITESPACE) {

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
    } while (bf_character_class(pos) == CCLASS_WHITESPACE);

    CharacterClass start_class = bf_character_class(pos);
    BufferPos look_ahead = buffer->pos;

    while (!bp_at_line_start(pos)) {
        RETURN_IF_FAIL(bf_change_char(buffer, &look_ahead, DIRECTION_LEFT, 0));

        if (start_class != bf_character_class(&look_ahead)) {
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

Status bf_insert_character(Buffer *buffer, const char *character, int advance_cursor)
{
    size_t char_len = 0;
    
    if (character != NULL) {
        char_len = strnlen(character, 5);
    }

    if (char_len == 0 || char_len > 4) {
        return st_get_error(ERR_INVALID_CHARACTER, "Invalid character %s", character);
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

    Range range;

    if (bf_get_range(buffer, &range)) {
        Status status = bf_delete_range(buffer, &range);
        RETURN_IF_FAIL(status);
    }

    BufferPos *pos = &buffer->pos;
    gb_set_point(buffer->data, pos->offset);
    size_t lines_before;

    if (advance_cursor) {
        lines_before = gb_lines(pos->data);
    }

    if (!gb_insert(buffer->data, string, string_length)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to insert text");
    }

    buffer->is_dirty = 1;

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

            while (pos->offset < end_offset) {
                RETURN_IF_FAIL(bf_change_char(buffer, pos, DIRECTION_RIGHT, 1));
            }
        }
    }

    return STATUS_SUCCESS;
} 

Status bf_delete_character(Buffer *buffer)
{
    Range range;

    if (bf_get_range(buffer, &range)) {
        return bf_delete_range(buffer, &range);
    }

    BufferPos *pos = &buffer->pos;

    if (bp_at_buffer_end(pos)) {
        return STATUS_SUCCESS;
    }

    CharInfo char_info;
    buffer->cef.char_info(&char_info, CIP_DEFAULT, *pos);

    gb_set_point(buffer->data, pos->offset);

    if (!gb_delete(buffer->data, char_info.byte_length)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to delete character");
    }

    buffer->is_dirty = 1;

    return STATUS_SUCCESS;
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

    gb_set_point(buffer->data, pos->offset);

    if (!gb_delete(buffer->data, delete_byte_num)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to delete text");
    }

    return STATUS_SUCCESS;
}

Status bf_select_all_text(Buffer *buffer)
{
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

    size_t range_size = range.end.offset - range.start.offset;

    assert(range_size > 0);

    text_selection->str = malloc(range_size + 1);

    if (text_selection->str == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy selected text");
    }

    size_t copied = gb_get_range(buffer->data, range.start.offset, text_selection->str, range_size);

    (void)copied;
    assert(copied == range_size);

    *(text_selection->str + range_size) = '\0';
    text_selection->str_len = range_size;

    return STATUS_SUCCESS;
}

Status bf_cut_selected_text(Buffer *buffer, TextSelection *text_selection)
{
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
    return bf_insert_string(buffer, text_selection->str, text_selection->str_len, 1);
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

