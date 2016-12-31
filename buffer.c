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
#include "file.h"
#include "config.h"
#include "encoding.h"

#define FILE_BUF_SIZE 1024
#define DETECT_FF_LINE_NUM 5

static Status bf_add_new_line_at_buffer_end(Buffer *);
static Status bf_input_stream_read(InputStream *, char buf[], size_t buf_len,
                                   size_t *bytes_read);
static Status bf_input_stream_close(InputStream *);
static Status bf_output_stream_write(OutputStream *, const char buf[],
                                     size_t buf_len, size_t *bytes_written);
static Status bf_output_stream_close(OutputStream *);
static int is_selection(Direction *);
static void bf_default_movement_selection_handler(Buffer *, int is_select,
                                                  Direction *);
static Status bf_change_real_line(Buffer *, BufferPos *, 
                                  Direction, int is_cursor);
static Status bf_change_screen_line(Buffer *, BufferPos *,
                                    Direction, int is_cursor);
static Status bf_advance_bp_to_line_offset(Buffer *, BufferPos *,
                                           int is_select);
static void bf_update_line_col_offset(Buffer *, const BufferPos *);
static Status bf_add_mark(Buffer *, Mark *);
static Mark *bf_get_mark(const Buffer *, const BufferPos *);
static int bf_remove_pos_mark(Buffer *, const BufferPos *, int free);
static int bf_remove_mark(Buffer *, Mark *, int free);
static Status bf_update_marks(Buffer *, const BufferPos *change_pos,
                              TextChangeType change_type, size_t change_length,
                              size_t change_lines);
static void bf_update_mark(Mark *, const BufferPos *change_pos,
                           TextChangeType change_type, size_t change_length,
                           size_t change_lines);
static Status bf_insert_expanded_tab(Buffer *, int advance_cursor);
static Status bf_auto_indent(Buffer *, int advance_cursor);
static Status bf_convert_fileformat(TextSelection *in_ts, TextSelection *out_ts, 
                                    int *conversion_perfomed);
static Status bf_mask_allows_input(const Buffer *, const char *str,
                                   size_t str_len, int *input_allowed);

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

    if ((buffer->marks = new_hashmap()) == NULL) {
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
    buffer->file_format = FF_UNIX;
    bp_init(&buffer->pos, buffer->data, &buffer->file_format, buffer->config);
    bp_init(&buffer->select_start, buffer->data,
            &buffer->file_format, buffer->config);
    bf_select_reset(buffer);
    bs_init_default_opt(&buffer->search);
    bc_init(&buffer->changes);
    buffer->change_state = bc_get_current_state(&buffer->changes);

    if ((buffer->bv = bv_new(24, 80, &buffer->pos)) == NULL) {
        bf_free(buffer);
        return NULL;
    }

    if (!STATUS_IS_SUCCESS(bf_add_new_mark(buffer, &buffer->bv->screen_start,
                                           MP_NO_ADJUST_ON_BUFFER_POS))) {
        bf_free(buffer);
        return 0;
    }

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
    free_hashmap_values(buffer->marks, (void (*)(void *))bp_free_mark);
    free_hashmap(buffer->marks);
    bv_free(buffer->bv);

    free(buffer);
}

void bf_free_syntax_match_cache(Buffer *buffer)
{
    bv_free_syntax_match_cache(buffer->bv);
}

Status bf_clear(Buffer *buffer)
{
    BufferPos *pos = &buffer->pos;
    bf_select_reset(buffer);
    bp_to_buffer_start(pos);
    return bf_delete(buffer, bf_length(buffer));
}

Status bf_reset(Buffer *buffer)
{
    bc_disable(&buffer->changes); 
    Status status = bf_clear(buffer);
    bc_enable(&buffer->changes);
    RETURN_IF_FAIL(status);

    bc_free(&buffer->changes);
    bc_init(&buffer->changes);

    return status;
}

/* Look at the first couple of lines to determine 
 * if buffer has Unix or windows line endings */
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

            point++;
        } while (--lines != 0);

        if (dos_le > unix_le) {
            file_format = FF_WINDOWS;
        }
    }

    return file_format;
}

/* Reset buffer and load configured file into buffer */
Status bf_load_file(Buffer *buffer)
{
    RETURN_IF_FAIL(bf_reset(buffer));

    if (!fi_file_exists(&buffer->file_info)) {
        /* If the file represented by this buffer doesn't exist
         * then the buffer content is empty */
        return STATUS_SUCCESS;
    }

    /* We don't want the inital load into the buffer to be undoable */
    bc_disable(&buffer->changes);

    Status status = bf_read_file(buffer, &buffer->file_info);

    bc_enable(&buffer->changes);

    return status;
}

/* Read file content info buffer at current position */
Status bf_read_file(Buffer *buffer, const FileInfo *file_info)
{
    if (!fi_file_exists(file_info)) {
        return st_get_error(ERR_FILE_DOESNT_EXIST, "File doesn't exist: %s",
                            file_info->rel_path);
    }

    FILE *input_file = fopen(file_info->abs_path, "rb");

    if (input_file == NULL) {
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE,
                            "Unable to open file %s for reading - %s", 
                            file_info->file_name, strerror(errno));
    } 

    size_t old_size = bf_length(buffer);
    size_t new_size = old_size + file_info->file_stat.st_size;

    /* Attempt to allocate necessary memory before loading into gap buffer */
    if (!gb_preallocate(buffer->data, new_size)) {
        return OUT_OF_MEMORY("File is too large to load into memory");
    }

    Status status = STATUS_SUCCESS;
    char buf[FILE_BUF_SIZE];
    size_t read;

    gb_set_point(buffer->data, buffer->pos.offset);

    do {
        read = fread(buf, sizeof(char), FILE_BUF_SIZE, input_file);

        if (ferror(input_file)) {
            status = st_get_error(ERR_UNABLE_TO_READ_FILE,
                                  "Unable to read from file %s - %s", 
                                  file_info->file_name, strerror(errno));
            break;
        } 

        /* Add text to gap buffer and advance internal point */
        if (!gb_add(buffer->data, buf, read)) {
            status = OUT_OF_MEMORY("Unable to populate buffer");
            break;
        }
    } while (read == FILE_BUF_SIZE);

    fclose(input_file);

    size_t bytes_inserted = bf_length(buffer) - old_size;

    if (bytes_inserted > 0) {
        ONLY_OVERWRITE_SUCCESS(
            status,
            bc_add_text_insert(&buffer->changes, bytes_inserted, &buffer->pos)
        );
    }

    bf_set_is_draw_dirty(buffer, 1);

    return status;
}

/* Add new line to buffer end if one doesn't exist */
static Status bf_add_new_line_at_buffer_end(Buffer *buffer)
{
    size_t buffer_len = gb_length(buffer->data);

    if (buffer_len == 0 ||
        gb_get_at(buffer->data, buffer_len - 1) == '\n') {
        return STATUS_SUCCESS;
    }

    const char *new_line = bf_new_line_str(buffer->file_format);

    BufferPos tmp = buffer->pos;
    bp_to_buffer_end(&buffer->pos);

    Status status = bf_insert_string(buffer, new_line, strlen(new_line), 0);

    buffer->pos = tmp;

    return status;
}

/* Write buffer to temporary file in same directory as file_path
 * then rename temporary file to file_path */
Status bf_write_file(Buffer *buffer, const char *file_path)
{
    assert(!is_null_or_empty(file_path));

    RETURN_IF_FAIL(bf_add_new_line_at_buffer_end(buffer));

    const FileInfo *file_info = &buffer->file_info;

    size_t tmp_file_path_len = strlen(file_path) + 6 + 1;
    char *tmp_file_path = malloc(tmp_file_path_len);

    if (tmp_file_path == NULL) {
        return OUT_OF_MEMORY("Unable to create temporary file path");
    }

    snprintf(tmp_file_path, tmp_file_path_len, "%sXXXXXX", file_path);

    int output_file = mkstemp(tmp_file_path);

    if (output_file == -1) {
        free(tmp_file_path);
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE,
                            "Unable to open temporary file for writing - %s",
                            strerror(errno));
    }

    Status status = STATUS_SUCCESS;
    size_t buffer_len = gb_length(buffer->data);
    size_t bytes_remaining = buffer_len;
    size_t bytes_retrieved;
    char buf[FILE_BUF_SIZE];

    /* Read text from gap buffer and write to temporary file */

    while (bytes_remaining > 0) {
        bytes_retrieved = gb_get_range(buffer->data,
                                       buffer_len - bytes_remaining,
                                       buf, FILE_BUF_SIZE);

        if (write(output_file, buf, bytes_retrieved) <= 0) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE,
                                  "Unable to write to temporary file - %s",
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
        /* Set permissions and ownership of temporary file
         * to match existing file */

        if (chmod(tmp_file_path, file_stat.st_mode) == -1) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE,
                                  "Unable to set file permissions - %s", 
                                  strerror(errno));
            goto cleanup;
        }

        if (chown(tmp_file_path, file_stat.st_uid, file_stat.st_gid) == -1) {
            status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE,
                                  "Unable to set owner - %s",
                                  strerror(errno));
            goto cleanup;
        }
    }

    /* Overwrite existing file atomically */
    if (rename(tmp_file_path, file_path) == -1) {
        status = st_get_error(ERR_UNABLE_TO_WRITE_TO_FILE,
                              "Unable to overwrite file %s - %s",
                              file_info->rel_path, strerror(errno)); 
    }

cleanup:
    if (STATUS_IS_SUCCESS(status)) {
        buffer->change_state = bc_get_current_state(&buffer->changes);
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

/* TODO This is a simple but somewhat inefficient implementation */
char *bf_join_lines_string(const Buffer *buffer, const char *seperator)
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

int bf_is_view_initialised(const Buffer *buffer)
{
    return buffer->bv != NULL;
}

int bf_is_dirty(const Buffer *buffer)
{
    return bc_has_state_changed(&buffer->changes, buffer->change_state);
}

int bf_is_draw_dirty(const Buffer *buffer)
{
    return buffer->is_draw_dirty;
}

void bf_set_is_draw_dirty(Buffer *buffer, int is_draw_dirty)
{
    buffer->is_draw_dirty = is_draw_dirty;
}

int bf_get_range(Buffer *buffer, Range *range)
{
    if (!bf_selection_started(buffer)) {
        return 0;
    } else if (bp_compare(&buffer->pos, &buffer->select_start) == 0) {
        /* Cannot have empty selection */
        bf_select_reset(buffer);
        return 0;
    }

    range->start = bp_min(&buffer->pos, &buffer->select_start);
    range->end = bp_max(&buffer->pos, &buffer->select_start);

    return 1;
}

int bf_bp_in_range(const Range *range, const BufferPos *pos)
{
    if (bp_compare(pos, &range->start) < 0 ||
        bp_compare(pos, &range->end) >= 0) {
        return 0;
    }

    return 1;
}

int bf_offset_in_range(const Range *range, size_t offset)
{
    return (offset >= range->start.offset && offset < range->end.offset);
}

static Status bf_input_stream_read(InputStream *is, char buf[], size_t buf_len,
                                   size_t *bytes_read)
{
    BufferInputStream *bis = (BufferInputStream *)is;
    size_t offset = bis->read_pos.offset;
    size_t limit_offset = bis->end_pos.offset;

    if (offset < limit_offset) {
        buf_len = MIN(buf_len, limit_offset - offset);
        *bytes_read = gb_get_range(bis->buffer->data, offset, buf, buf_len);
        bis->read_pos.offset += *bytes_read;
    } else {
        *bytes_read = 0;
    }

    return STATUS_SUCCESS;
}

static Status bf_input_stream_close(InputStream *is)
{
    BufferInputStream *bis = (BufferInputStream *)is;
    bf_remove_pos_mark(bis->buffer, &bis->read_pos, 1);
    bf_remove_pos_mark(bis->buffer, &bis->end_pos, 1);

    return STATUS_SUCCESS;
}

Status bf_get_buffer_input_stream(BufferInputStream *bis, Buffer *buffer,
                                  const Range *range)
{
    *bis = (BufferInputStream) {
        .is = {
            .read = bf_input_stream_read,
            .close = bf_input_stream_close
        },
        .buffer = buffer,
        .read_pos = range->start,
        .end_pos = range->end
    };

    RETURN_IF_FAIL(bf_add_new_mark(buffer, &bis->read_pos,
                                   MP_ADJUST_OFFSET_ONLY));
    Status status = bf_add_new_mark(buffer, &bis->end_pos,
                                    MP_ADJUST_OFFSET_ONLY);

    if (!STATUS_IS_SUCCESS(status)) {
        bf_remove_pos_mark(buffer, &bis->read_pos, 1);
    }

    return status;
}

static Status bf_output_stream_write(OutputStream *os, const char buf[],
                                     size_t buf_len, size_t *bytes_written)
{
    BufferOutputStream *bos = (BufferOutputStream *)os;
    BufferPos pos = bos->buffer->pos;
    bos->buffer->pos = bos->write_pos;
    size_t replace_len;

    if (bos->replace_mode) {
        size_t max_replace_len = bf_length(bos->buffer) - bos->write_pos.offset;
        replace_len = MIN(max_replace_len, buf_len);
    } else {
        replace_len = 0;
    }

    Status status = bf_replace_string(bos->buffer, replace_len,
                                      buf, buf_len, 1);

    if (STATUS_IS_SUCCESS(status)) {
        bos->write_pos = bos->buffer->pos;
        *bytes_written = buf_len;
    }

    bos->buffer->pos = pos;

    return status;
}

static Status bf_output_stream_close(OutputStream *os)
{
    (void)os;
    return STATUS_SUCCESS;
}

Status bf_get_buffer_output_stream(BufferOutputStream *bos, Buffer *buffer,
                                   const BufferPos *write_pos,
                                   int replace_mode)
{
    *bos = (BufferOutputStream) {
        .os = {
            .write = bf_output_stream_write,
            .close = bf_output_stream_close
        },
        .buffer = buffer,
        .write_pos = *write_pos,
        .replace_mode = replace_mode
    };

    return STATUS_SUCCESS;
}

/* TODO Consider UTF-8 punctuation and whitespace */
CharacterClass bf_character_class(const Buffer *buffer, const BufferPos *pos)
{
    CharInfo char_info;
    en_utf8_char_info(&char_info, CIP_DEFAULT, pos, buffer->config);

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

FileFormat bf_get_fileformat(const Buffer *buffer)
{
    return buffer->file_format;
}

int bf_determine_fileformat(const char *ff_name, FileFormat *file_format)
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

const char *bf_determine_fileformat_str(FileFormat file_format)
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
        size_t screen_col_no = (pos->col_no - 1) % buffer->bv->cols;

        if (screen_col_no == 0) {
            return 1;
        }

        BufferPos prev_char = *pos;
        bp_prev_char(&prev_char);

        size_t prev_screen_col_no = 
            (prev_char.col_no - 1) % buffer->bv->cols;

        if (prev_screen_col_no == 0) {
            return 0;
        }

        /* Handle screen lines that end with characters that
         * take up multiple columns and wrap onto the next
         * screen line */
        return screen_col_no < prev_screen_col_no;
    }

    return bp_at_line_start(pos);
}

int bf_bp_at_screen_line_end(const Buffer *buffer, const BufferPos *pos)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        size_t screen_col_no = pos->col_no % buffer->bv->cols;

        if (screen_col_no == 0) {
            return 1;
        }

        BufferPos next_char = *pos;
        bp_next_char(&next_char);

        size_t next_screen_col_no = next_char.col_no % buffer->bv->cols;

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

/* Zero out DIRECTION_WITH_SELECT bit and return true if it was set */
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
    /* select_start.line_no is set to 0 to
     * indicate no selection exists */
    return buffer->select_start.line_no > 0;
}

static void bf_default_movement_selection_handler(Buffer *buffer, int is_select,
                                                  Direction *direction)
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

Status bf_set_bp(Buffer *buffer, const BufferPos *pos, int is_select)
{
    assert(pos->data == buffer->data);
    assert(pos->offset <= gb_length(pos->data));

    if (pos->data != buffer->data ||
        pos->offset > gb_length(pos->data)) {
        return st_get_error(ERR_INVALID_BUFFERPOS, "Invalid Buffer Position");
    }

    buffer->pos = *pos;

    if (is_select) {
        bf_select_continue(buffer);
    } else if (bf_selection_started(buffer)) {
        bf_select_reset(buffer);
    }

    bf_update_line_col_offset(buffer, &buffer->pos);

    return STATUS_SUCCESS;
}

/* Move cursor up or down a line keeping the offset into the line the same 
 * or as close to the original if possible */
Status bf_change_line(Buffer *buffer, BufferPos *pos, Direction direction,
                      int is_cursor)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        return bf_change_screen_line(buffer, pos, direction, is_cursor);
    }

    return bf_change_real_line(buffer, pos, direction, is_cursor);
}

static Status bf_change_real_line(Buffer *buffer, BufferPos *pos, 
                                  Direction direction, int is_cursor)
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

/* Move cursor up or down a screen line keeping the cursor column as close to 
 * the starting value as possible. For lines which don't wrap this function 
 * behaves the same as by bf_change_real_line. For lines which wrap this 
 * allows a user to scroll up or down to a different part of the line displayed 
 * as a different line on the screen. Therefore this function is dependent 
 * on the width of the screen. */
static Status bf_change_screen_line(Buffer *buffer, BufferPos *pos, 
                                    Direction direction, int is_cursor)
{
    int is_select = is_selection(&direction);

    assert(direction == DIRECTION_UP || direction == DIRECTION_DOWN);

    if (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN)) {
        return STATUS_SUCCESS;
    }

    Direction pos_direction = (direction == DIRECTION_DOWN ? 
                               DIRECTION_RIGHT : DIRECTION_LEFT);

    if (is_cursor) {
        bf_default_movement_selection_handler(buffer, is_select,
                                              &pos_direction);
    }

    size_t start_col = bv_screen_col_no(buffer, pos);

    if (direction == DIRECTION_UP) {
        if (!bf_bp_at_screen_line_start(buffer, pos)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_start(buffer, pos, 
                                                      is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (!bf_bp_at_screen_line_start(buffer, pos) &&
               bv_screen_col_no(buffer, pos) > start_col) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));
        }
    } else {
        if (!bf_bp_at_screen_line_end(buffer, pos)) {
            RETURN_IF_FAIL(bf_bp_to_screen_line_end(buffer, pos, is_select, 0));
        }

        RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));

        while (!bp_at_line_end(pos) &&
               !bf_bp_at_screen_line_end(buffer, pos) &&
               bv_screen_col_no(buffer, pos) < start_col) {
            RETURN_IF_FAIL(bf_change_char(buffer, pos, pos_direction, 0));
        }
    }

    if (is_cursor) {
        RETURN_IF_FAIL(bf_advance_bp_to_line_offset(buffer, pos, is_select));
    }

    return STATUS_SUCCESS;
}

static Status bf_advance_bp_to_line_offset(Buffer *buffer, BufferPos *pos,
                                           int is_select)
{
    size_t global_col_offset = buffer->line_col_offset;
    size_t current_col_offset = bv_screen_col_no(buffer, pos) - 1;
    Direction direction = DIRECTION_RIGHT;

    if (is_select) {
        direction |= DIRECTION_WITH_SELECT;
    }

    size_t last_col;
    Status status = STATUS_SUCCESS;

    while (STATUS_IS_SUCCESS(status) &&
           !bp_at_line_end(pos) &&
           current_col_offset < global_col_offset) {
        last_col = pos->col_no;
        status = bf_change_char(buffer, pos, direction, 1);
        current_col_offset += pos->col_no - last_col;
    }

    buffer->line_col_offset = global_col_offset;

    return status;
}

Status bf_change_multi_line(Buffer *buffer, BufferPos *pos, Direction direction,
                            size_t offset, int is_cursor)
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
Status bf_change_char(Buffer *buffer, BufferPos *pos, Direction direction,
                      int is_cursor)
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
            /* Clear selection and move to whichever selection
             * end point is in the correct direction */
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

Status bf_change_multi_char(Buffer *buffer, BufferPos *pos, Direction direction,
                            size_t offset, int is_cursor)
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

/* Keep track of offset into line so that it 
 * can be persisted when changing line */
static void bf_update_line_col_offset(Buffer *buffer, const BufferPos *pos)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        if (buffer->bv != NULL) {
            buffer->line_col_offset = (pos->col_no - 1) % buffer->bv->cols;
        }
    } else {
        buffer->line_col_offset = pos->col_no - 1;
    }
}

Status bf_to_line_start(Buffer *buffer, BufferPos *pos, int is_select,
                        int is_cursor)
{
    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        return bf_bp_to_screen_line_start(buffer, pos, is_select, is_cursor);
    }

    return bf_bp_to_line_start(buffer, pos, is_select, is_cursor);
}

Status bf_bp_to_line_start(Buffer *buffer, BufferPos *pos, int is_select,
                           int is_cursor)
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

Status bf_bp_to_screen_line_start(Buffer *buffer, BufferPos *pos,
                                  int is_select, int is_cursor)
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

Status bf_bp_to_line_end(Buffer *buffer, BufferPos *pos, int is_select,
                         int is_cursor)
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

Status bf_bp_to_screen_line_end(Buffer *buffer, BufferPos *pos, int is_select,
                                int is_cursor)
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

/* Our definition of word includes the underscore character intentionally */
Status bf_to_next_word(Buffer *buffer, int is_select)
{
    Direction direction = DIRECTION_RIGHT;
    bf_default_movement_selection_handler(buffer, is_select, &direction);

    BufferPos *pos = &buffer->pos;
    Status status;

    /* Behaviour is different when selecting text */
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

Status bf_to_next_paragraph(Buffer *buffer, int is_select)
{
    bf_default_movement_selection_handler(buffer, is_select, NULL);
    BufferPos *pos = &buffer->pos;
    int prev_line_whitespace = bp_on_whitespace_line(pos);

    while (bp_next_line(pos)) {
        if (bp_on_whitespace_line(pos)) {
            prev_line_whitespace = 1;
        } else if (prev_line_whitespace) {
            bf_advance_bp_to_line_offset(buffer, pos, is_select);
            break;
        } else {
            prev_line_whitespace = 0;
        }
    }

    return STATUS_SUCCESS;
}

Status bf_to_prev_paragraph(Buffer *buffer, int is_select)
{
    bf_default_movement_selection_handler(buffer, is_select, NULL);
    BufferPos *pos = &buffer->pos;
    bp_prev_line(pos);
    int prev_line_whitespace = bp_on_whitespace_line(pos);

    while (bp_prev_line(pos)) {
        if (!bp_on_whitespace_line(pos)) {
            prev_line_whitespace = 0;
        } else if (!prev_line_whitespace) {
            bp_next_line(pos);
            bf_advance_bp_to_line_offset(buffer, pos, is_select);
            break;
        } else {
            prev_line_whitespace = 1;
        }
    }

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

    Status status = bf_change_multi_line(buffer, pos, direction,
                                         buffer->bv->rows - 1, 1);

    RETURN_IF_FAIL(status);

    if (buffer->bv->screen_start.line_no != buffer->pos.line_no) {
        buffer->bv->screen_start = buffer->pos;
        RETURN_IF_FAIL(bf_bp_to_screen_line_start(buffer,
                                                  &buffer->bv->screen_start,
                                                  0, 0));
        bf_set_is_draw_dirty(buffer, 1);
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

Status bf_add_new_mark(Buffer *buffer, BufferPos *pos,
                              MarkProperties prop)
{
    Mark *mark = bp_new_mark(pos, prop);

    if (mark == NULL) {
        return OUT_OF_MEMORY("Unable allocate mark" );
    }

    Status status = bf_add_mark(buffer, mark);

    if (!STATUS_IS_SUCCESS(status)) {
        bp_free_mark(mark);
    }

    return status;
}

static Status bf_add_mark(Buffer *buffer, Mark *mark)
{
    if (mark == NULL) {
        return st_get_error(ERR_INVALID_MARK, "NULL mark");        
    }

    char addr[50];
    snprintf(addr, sizeof(addr), "%p", (void *)mark->pos);

    if (hashmap_get(buffer->marks, addr) != NULL) {
        return st_get_error(ERR_DUPLICATE_MARK, "Mark already tracked");
    } else if (!hashmap_set(buffer->marks, addr, mark)) {
        return OUT_OF_MEMORY("Unable to save mark" );
    }

    return STATUS_SUCCESS;
}

static Mark *bf_get_mark(const Buffer *buffer, const BufferPos *pos)
{
    char addr[50];
    snprintf(addr, sizeof(addr), "%p", (void *)pos);

    return hashmap_get(buffer->marks, addr);
}

static int bf_remove_pos_mark(Buffer *buffer, const BufferPos *pos, int free)
{
    Mark *mark = bf_get_mark(buffer, pos);
    return bf_remove_mark(buffer, mark, free);
}

static int bf_remove_mark(Buffer *buffer, Mark *mark, int free)
{
    if (mark == NULL) {
        return 0;
    }

    char addr[50];
    snprintf(addr, sizeof(addr), "%p", (void *)mark->pos);

    int deleted = hashmap_delete(buffer->marks, addr);

    if (free) {
        bp_free_mark(mark);
    }

    return deleted;
}

static Status bf_update_marks(Buffer *buffer, const BufferPos *change_pos,
                              TextChangeType change_type, size_t change_length,
                              size_t change_lines)
{
    /* TODO Need HashMapIterator implementation to avoid heap allocation
     * just to loop through hash entries */
    const char **mark_refs = hashmap_get_keys(buffer->marks);

    if (mark_refs == NULL) {
        return OUT_OF_MEMORY("Unable to allocate mark list");
    }

    size_t mark_num = hashmap_size(buffer->marks);
    Mark *mark;

    for (size_t k = 0; k < mark_num; k++) {
        mark = (Mark *)hashmap_get(buffer->marks, mark_refs[k]);
        assert(mark != NULL);

        if (mark != NULL) {
            bf_update_mark(mark, change_pos, change_type,
                           change_length, change_lines);
        }
    }

    free(mark_refs);

    return STATUS_SUCCESS;
}

static void bf_update_mark(Mark *mark, const BufferPos *change_pos,
                           TextChangeType change_type, size_t change_length,
                           size_t change_lines)
{
    BufferPos *mark_pos = mark->pos;

    if (mark_pos->line_no == 0 || change_length == 0 ||
        mark_pos->offset < change_pos->offset ||
        ((mark->prop & MP_NO_ADJUST_ON_BUFFER_POS) &&
         mark_pos->offset == change_pos->offset)) {
        return;
    }

    if (change_type == TCT_INSERT) {
        mark_pos->offset += change_length;

        if (!(mark->prop & MP_ADJUST_OFFSET_ONLY)) {
            if (mark_pos->line_no == change_pos->line_no) {
                bp_recalc_col(mark_pos);
            }

            mark_pos->line_no += change_lines;
        }
    } else if (change_type == TCT_DELETE) {
        if (mark_pos->offset < change_pos->offset + change_length) {
            *mark_pos = *change_pos;
        } else {
            assert(mark_pos->offset >= change_length);
            mark_pos->offset -= change_length;

            if (!(mark->prop & MP_ADJUST_OFFSET_ONLY)) {
                if (mark_pos->line_no <= change_pos->line_no + change_lines) {
                    bp_recalc_col(mark_pos);
                }

                mark_pos->line_no -= change_lines;
            }
        }
    } else {
        assert(!"Invalid TextChangeType");
    }

    assert(mark_pos->offset <= gb_length(mark_pos->data));
    assert(mark->prop & MP_ADJUST_OFFSET_ONLY ||
           mark_pos->line_no <= gb_lines(mark_pos->data) + 1);
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

    /* If there's no whitespace at the start of the line then
     * just insert a new line character */
    if (!(tmp.offset > line_start_offset)) {
        return bf_insert_string(buffer, new_line, new_line_len, advance_cursor);
    }

    size_t indent_length = tmp.offset - line_start_offset;
    char *indent = malloc(indent_length + new_line_len);

    if (indent == NULL) {
        return OUT_OF_MEMORY("Unable to insert character");
    }

    gb_get_range(buffer->data, line_start_offset,
                 indent + new_line_len, indent_length);
    memcpy(indent, new_line, new_line_len);
    size_t insert_length = indent_length + new_line_len;

    Status status = bf_insert_string(buffer, indent, insert_length,
                                     advance_cursor);
    free(indent);

    return status;
}

/* This function performs expand tab and auto indent functionality that
 * bf_insert_string doesn't */
Status bf_insert_character(Buffer *buffer, const char *character,
                           int advance_cursor)
{
    size_t char_len = 0;
    
    if (character != NULL) {
        char_len = strnlen(character, 5);
    }

    /* UTF-8 characters should be no longer than 4 bytes */
    if (char_len == 0 || char_len > 4) {
        return st_get_error(ERR_INVALID_CHARACTER,
                            "Invalid character %s", character);
    }

    if (*character == '\t' && cf_bool(buffer->config, CV_EXPANDTAB)) {
        return bf_insert_expanded_tab(buffer, advance_cursor);
    } else if (*character == '\n') {
        if (cf_bool(buffer->config, CV_AUTOINDENT)) {
            return bf_auto_indent(buffer, advance_cursor);
        } else {
            character = bf_new_line_str(buffer->file_format);
            char_len = strlen(character);
        }
    }

    return bf_insert_string(buffer, character, char_len, advance_cursor);
}

Status bf_insert_string(Buffer *buffer, const char *string,
                        size_t string_length, int advance_cursor)
{
    if (string == NULL) {
        return st_get_error(ERR_INVALID_CHARACTER, "Cannot insert NULL string");
    } else if (string_length == 0) {
        return STATUS_SUCCESS;
    } else if (bf_has_mask(buffer)) {
        int input_allowed;
        RETURN_IF_FAIL(bf_mask_allows_input(buffer, string, string_length,
                                            &input_allowed));

        if (!input_allowed) {
            return STATUS_SUCCESS;
        }
    }

    Status status = STATUS_SUCCESS;

    Range range;
    int grouped_changes_started = 0;

    /* Check if text is selected */
    if (bf_get_range(buffer, &range)) {
        /* If insert/delete changes are not already being grouped
         * then start grouping them. This allows the undo operation to
         * remove the inserted string and insert the selected text again */
        if (!bc_grouped_changes_started(&buffer->changes)) {
            RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));
            grouped_changes_started = 1;
        }

        /* Delete selection */
        status = bf_delete_range(buffer, &range);
        
        if (!STATUS_IS_SUCCESS(status)) {
            goto cleanup;
        }
    }

    BufferPos *pos = &buffer->pos;
    BufferPos start_pos = *pos;
    gb_set_point(buffer->data, pos->offset);
    size_t lines_before = gb_lines(pos->data);

    if (!gb_insert(buffer->data, string, string_length)) {
        status = OUT_OF_MEMORY("Unable to insert text");
        goto cleanup;
    }

    size_t lines_after = gb_lines(buffer->data);
    buffer->is_draw_dirty = 1;

    bf_update_marks(buffer, &buffer->pos, TCT_INSERT, string_length,
                    lines_after - lines_before);

    status = bc_add_text_insert(&buffer->changes, string_length, &start_pos);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    if (advance_cursor) {
        /* TODO Somewhat arbitrary length chosen here */
        if (string_length > 100) {
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

/* This operation is just a delete and insert grouped together */
Status bf_replace_string(Buffer *buffer, size_t replace_length,
                         const char *string, size_t string_length,
                         int advance_cursor)
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
        status = bf_insert_string(buffer, string, string_length,
                                  advance_cursor);
    }

    if (!grouped_changes_started) {
        status = bc_end_grouped_changes(&buffer->changes);
    }

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
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

    char *deleted_str = NULL;
    int undo_enabled = bc_enabled(&buffer->changes);

    if (undo_enabled) {
        deleted_str = malloc(byte_num);

        if (deleted_str == NULL) {
            return OUT_OF_MEMORY("Unable to save deletion");
        }

        gb_get_range(buffer->data, pos->offset, deleted_str, byte_num);
    }

    size_t lines_before = gb_lines(pos->data);
    gb_set_point(buffer->data, pos->offset);

    if (!gb_delete(buffer->data, byte_num)) {
        return OUT_OF_MEMORY("Unable to delete character");
    }

    size_t lines_after = gb_lines(buffer->data);
    buffer->is_draw_dirty = 1;

    bf_update_marks(buffer, &buffer->pos, TCT_DELETE, byte_num,
                    lines_before - lines_after);

    Status status = STATUS_SUCCESS;

    if (undo_enabled) {
        status = bc_add_text_delete(&buffer->changes, deleted_str,
                                    byte_num, pos); 

        free(deleted_str);
    }

    bf_update_line_col_offset(buffer, pos);

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
        en_utf8_char_info(&char_info, CIP_DEFAULT, 
                          &buffer->pos, buffer->config);
        byte_length = char_info.byte_length;
    }

    return bf_delete(buffer, byte_length);
}

Status bf_select_continue(Buffer *buffer)
{
    /* If selection not started then start it from this position */
    if (!bf_selection_started(buffer)) {
        buffer->select_start = buffer->pos;
    }

    bf_set_is_draw_dirty(buffer, 1);

    return STATUS_SUCCESS;
}

Status bf_select_reset(Buffer *buffer)
{
    if (bf_selection_started(buffer)) {
        buffer->select_start.line_no = 0;
        bf_set_is_draw_dirty(buffer, 1);
    }

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

    bf_select_reset(buffer);
    bp_to_buffer_end(&buffer->select_start);
    bp_to_buffer_start(&buffer->pos);
    bf_select_continue(buffer);

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
        return OUT_OF_MEMORY("Unable to copy selected text");
    }

    size_t copied = gb_get_range(buffer->data, range.start.offset,
                                 text_selection->str, range_size);

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

Status bf_insert_textselection(Buffer *buffer, TextSelection *text_selection,
                               int advance_cursor)
{
    assert(text_selection->str != NULL);
    
    if (text_selection->str_len == 0) {
        return STATUS_SUCCESS;
    }

    /* Convert from Windows to Unix line endings and vice versa
     * if necessary. Usually occurs when copying between buffers */
    if (text_selection->file_format != buffer->file_format) {
        TextSelection converted_selection = {
            .file_format = buffer->file_format
        };
        int conversion_perfomed;

        Status status = bf_convert_fileformat(text_selection,
                                              &converted_selection,
                                              &conversion_perfomed);
        RETURN_IF_FAIL(status);

        if (conversion_perfomed) {
            status = bf_insert_string(buffer, converted_selection.str, 
                                      converted_selection.str_len,
                                      advance_cursor);

            bf_free_textselection(&converted_selection);
            return status;
        }
    }

    return bf_insert_string(buffer, text_selection->str, 
                            text_selection->str_len, advance_cursor);
}

static Status bf_convert_fileformat(TextSelection *in_ts, TextSelection *out_ts,
                                    int *conversion_perfomed)
{
    *conversion_perfomed = 0;

    /* If the formats are the same or the text selection contains 
     * no new line characters then do nothing */
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
        return OUT_OF_MEMORY("Unable to convert fileformat of text");
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

Status bf_reset_with_text(Buffer *buffer, const char *text)
{
    RETURN_IF_FAIL(bf_reset(buffer));

    if (text != NULL) {
        return bf_insert_string(buffer, text, strlen(text), 1);
    }

    return STATUS_SUCCESS;
}

Status bf_set_mask(Buffer *buffer, const Regex *regex)
{
    bf_remove_mask(buffer);
    return ru_compile(&buffer->mask, regex);
}

int bf_has_mask(const Buffer *buffer)
{
    return buffer->mask.regex != NULL;
}

static Status bf_mask_allows_input(const Buffer *buffer, const char *str,
                                   size_t str_len, int *input_allowed)
{
    assert(bf_has_mask(buffer));

    if (str_len == 0) {
        *input_allowed = 0;
        return STATUS_SUCCESS; 
    }

    RegexResult result;
    RETURN_IF_FAIL(ru_exec(&result, &buffer->mask, str, str_len, 0));
    *input_allowed = result.match;
    
    return STATUS_SUCCESS;
}

void bf_remove_mask(Buffer *buffer)
{
    if (!bf_has_mask(buffer)) {
        return;
    }

    ru_free_instance(&buffer->mask);
    memset(&buffer->mask, 0, sizeof(RegexInstance));
}

Status bf_goto_line(Buffer *buffer, size_t line_no)
{
    buffer->pos = bp_init_from_line_col(line_no, 1, &buffer->pos);
    bf_update_line_col_offset(buffer, &buffer->pos);
    return STATUS_SUCCESS;
}

/* Move line or lines covered by selection up or down buffer */
Status bf_vert_move_lines(Buffer *buffer, Direction direction)
{
    assert(direction == DIRECTION_UP || direction == DIRECTION_DOWN);

    if (!(direction == DIRECTION_UP || direction == DIRECTION_DOWN)) {
        return STATUS_SUCCESS;
    }

    Range range;
    int is_selection = 1;
    
    if (!bf_get_range(buffer, &range)) {
        range.start = range.end = buffer->pos;
        is_selection = 0;
    }

    /* Extend range to include all parts of selected lines */
    bp_to_line_start(&range.start);
    bp_to_line_end(&range.end);

    if ((direction == DIRECTION_UP &&
         bp_at_first_line(&range.start)) ||
        (direction == DIRECTION_DOWN &&
         bp_at_last_line(&range.end))) {
        return STATUS_SUCCESS;
    }

    RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));
    Status status;

    if (bp_at_last_line(&range.start)) {
        status = bf_add_new_line_at_buffer_end(buffer);

        if (!STATUS_IS_SUCCESS(status)) {
            goto cleanup;
        }
    }

    bp_next_char(&range.end);

    if (bp_at_last_line(&range.end)) {
        status = bf_add_new_line_at_buffer_end(buffer);

        if (!STATUS_IS_SUCCESS(status)) {
            goto cleanup;
        }
    }

    buffer->pos = range.start;
    buffer->select_start = range.end;

    TextSelection text_selection;
    status = bf_cut_selected_text(buffer, &text_selection);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    if (direction == DIRECTION_UP) {
        if (!bp_prev_line(&buffer->pos)) {
            bp_to_buffer_start(&buffer->pos);
        }
    } else {
        if (!bp_next_line(&buffer->pos)) {
            bp_to_buffer_end(&buffer->pos);
        }
    }

    status = bf_insert_textselection(buffer, &text_selection, 0);
    bf_free_textselection(&text_selection);

    if (STATUS_IS_SUCCESS(status)) {
        if (is_selection) {
            size_t select_start_offset = buffer->pos.offset + 
                                         (range.end.offset - 1
                                          - range.start.offset);
            buffer->select_start = bp_init_from_offset(select_start_offset,
                                                       &buffer->pos);
        } else {
            bf_select_reset(buffer);
        }
    }

cleanup:
    bc_end_grouped_changes(&buffer->changes);

    return status;
}

/* Indent or unindent range of lines */
Status bf_indent(Buffer *buffer, Direction direction)
{
    assert(direction == DIRECTION_RIGHT || direction == DIRECTION_LEFT);

    if (!(direction == DIRECTION_RIGHT || direction == DIRECTION_LEFT)) {
        return STATUS_SUCCESS;
    }

    Range range;
    
    if (!bf_get_range(buffer, &range) ||
        range.end.line_no == range.start.line_no) {
        return STATUS_SUCCESS;
    }

    RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));

    bp_to_line_start(&range.start);
    bp_to_line_end(&range.end);

    int pos_at_range_start = bp_compare(&buffer->select_start,
                                        &buffer->pos) > 0;

    Status status = STATUS_SUCCESS;

    buffer->pos = range.start;
    bf_select_reset(buffer);
    size_t lines = range.end.line_no - range.start.line_no;
    size_t start_bytes = bf_length(buffer);

    assert(lines > 0);

    /* Loop through each line in selection and either
     * indent or unindent as required */
    for (size_t k = 0; k <= lines; k++) {
        assert(buffer->pos.col_no == 1);

        switch (direction) {
            case DIRECTION_RIGHT: 
                {
                    status = bf_insert_character(buffer, "\t", 0);
                    break;
                }
            case DIRECTION_LEFT:
                {
                    CharInfo char_info;
                    size_t space_remaining = cf_int(buffer->config,
                                                    CV_TABWIDTH);

                    while (bf_character_class(buffer, &buffer->pos) 
                           == CCLASS_WHITESPACE &&
                           space_remaining != 0 &&
                           !bp_at_line_end(&buffer->pos)) {
                        
                        en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH,
                                          &buffer->pos, buffer->config);

                        if (char_info.screen_length <= space_remaining) {
                            status = bf_delete_character(buffer);

                            if (!STATUS_IS_SUCCESS(status)) {
                                break; 
                            }

                            space_remaining -= char_info.screen_length;
                        } else {
                            break;
                        }
                    }

                    break;
                }
            default:
                {
                    assert(!"Invalid direction");
                    break;
                }
        }

        if (!STATUS_IS_SUCCESS(status)) {
            break;
        }

        if (!bp_next_line(&buffer->pos)) {
            bp_to_line_end(&buffer->pos);
            bp_next_char(&buffer->pos);
        }
    }

    if (STATUS_IS_SUCCESS(status)) {
        size_t end_bytes = bf_length(buffer);

        if (direction == DIRECTION_RIGHT) {
            assert(end_bytes > start_bytes);
            range.end.offset += end_bytes - start_bytes;            
        } else {
            assert(start_bytes >= end_bytes);
            range.end.offset -= start_bytes - end_bytes;            
        }

        bp_recalc_col(&range.end);

        if (pos_at_range_start) {
            buffer->pos = range.start;
            buffer->select_start = range.end;
        } else {
            buffer->pos = range.end;
            buffer->select_start = range.start;
        }
    }

    bc_end_grouped_changes(&buffer->changes);

    return status;
}

/* Simple function for jumping to matching brackets.
 * TODO Doesn't take strings that could contain brackets into
 * account e.g. "}", which we generally want to avoid */
Status bf_jump_to_matching_bracket(Buffer *buffer)
{
    size_t offset = buffer->pos.offset;
    char current_char = gb_get_at(buffer->data, offset);
    char search_char;
    Direction direction;

    switch (current_char) {
        case '{':
            {
                search_char = '}';
                direction = DIRECTION_RIGHT;
                break;
            }
        case '}':
            {
                search_char = '{';
                direction = DIRECTION_LEFT;
                break;
            }
        case '[':
            {
                search_char = ']';
                direction = DIRECTION_RIGHT;
                break;
            }
        case ']':
            {
                search_char = '[';
                direction = DIRECTION_LEFT;
                break;
            }
        case '(':
            {
                search_char = ')';
                direction = DIRECTION_RIGHT;
                break;
            }
        case ')':
            {
                search_char = '(';
                direction = DIRECTION_LEFT;
                break;
            }
        case '<':
            {
                search_char = '>';
                direction = DIRECTION_RIGHT;
                break;
            }
        case '>':
            {
                search_char = '<';
                direction = DIRECTION_LEFT;
                break;
            }
        default:
            {
                return STATUS_SUCCESS;
            }
    }

    size_t buffer_len = bf_length(buffer);
    size_t scope = 0;
    int match_found = 0;
    char iter;

    if (direction == DIRECTION_RIGHT) {
        while (++offset < buffer_len) {
            iter = gb_get_at(buffer->data, offset);

            if (iter == current_char) {
                scope++;
            } else if (iter == search_char) {
                if (scope == 0) {
                    match_found = 1;
                    break;
                } else {
                    scope--;
                }
            }
        }
    } else {
        while (offset > 0) {
            iter = gb_get_at(buffer->data, --offset);

            if (iter == current_char) {
                scope++;
            } else if (iter == search_char) {
                if (scope == 0) {
                    match_found = 1;
                    break;
                } else {
                    scope--;
                }
            }
        }
    }

    if (!match_found) {
        return STATUS_SUCCESS;
    }

    BufferPos pos = bp_init_from_offset(offset, &buffer->pos);
    
    return bf_set_bp(buffer, &pos, 0);
}

/* Duplicate line or selected lines underneath */
Status bf_duplicate_selection(Buffer *buffer)
{
    Range range;
    
    if (!bf_get_range(buffer, &range)) {
        range.start = range.end = buffer->pos;
    }
    
    bp_to_line_start(&range.start);
    bp_to_line_end(&range.end);
    const char *new_line = bf_new_line_str(buffer->file_format);

    /* Empty line */
    if (bp_compare(&range.start, &range.end) == 0) {
        return bf_insert_string(buffer, new_line, strlen(new_line), 0);
    }

    RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));

    BufferPos pos = buffer->pos;
    BufferPos select_start = buffer->select_start;

    buffer->select_start = range.start;
    buffer->pos = range.end;

    TextSelection text_selection;
    Status status = bf_copy_selected_text(buffer, &text_selection);

    if (STATUS_IS_SUCCESS(status)) {
        bf_select_reset(buffer);

        status = bf_insert_string(buffer, new_line, strlen(new_line), 1);

        if (STATUS_IS_SUCCESS(status)) {
            status = bf_insert_textselection(buffer, &text_selection, 0);
        }
    }

    bf_free_textselection(&text_selection);
    bc_end_grouped_changes(&buffer->changes);

    buffer->pos = pos;
    buffer->select_start = select_start;

    return status;
}

Status bf_join_lines(Buffer *buffer, const char *sep, size_t sep_len)
{
    Range range;
    
    if (!bf_get_range(buffer, &range)) {
        range.start = range.end = buffer->pos;
    }
    
    if (range.start.line_no == bf_lines(buffer)) {
        return STATUS_SUCCESS;
    }

    RETURN_IF_FAIL(bc_start_grouped_changes(&buffer->changes));

    size_t new_line_len = strlen(bf_new_line_str(buffer->file_format));
    size_t line_range = range.end.line_no - range.start.line_no;

    if (line_range == 0) {
        line_range++;
    }

    buffer->pos = range.start;
    BufferPos *pos = &buffer->pos;
    bf_select_reset(buffer);
    Status status = STATUS_SUCCESS;

    for (size_t k = 0; STATUS_IS_SUCCESS(status) && k < line_range; k++) {
        bp_to_line_end(pos);
        status = bf_delete(buffer, new_line_len);

        while (STATUS_IS_SUCCESS(status) && !bp_at_line_end(pos) &&
               bf_character_class(buffer, pos) == CCLASS_WHITESPACE) {
            status = bf_delete_character(buffer);
        }

        if (!bp_at_line_end(pos) && STATUS_IS_SUCCESS(status)) {
            status = bf_insert_string(buffer, sep, sep_len, 0);
        }
    }

    bc_end_grouped_changes(&buffer->changes);

    return status;
}

size_t bf_get_text(const Buffer *buffer, const BufferPos *pos, char *buf,
                   size_t text_len)
{
    size_t buffer_len = bf_length(buffer);

    if (pos->offset > buffer_len) {
        return 0;
    }

    size_t end_pos = pos->offset + text_len;

    if (end_pos > buffer_len) {
        text_len -= (end_pos - buffer_len);
    }

    return gb_get_range(buffer->data, pos->offset, buf, text_len);
}

size_t bf_get_line(const Buffer *buffer, const BufferPos *pos, char *buf,
                   size_t buf_len)
{
    BufferPos start = *pos;
    BufferPos end = *pos;

    bp_to_line_start(&start);
    bp_to_line_end(&end);

    size_t text_len = MIN(end.offset - start.offset, buf_len);
    
    return bf_get_text(buffer, &start, buf, text_len);
}

