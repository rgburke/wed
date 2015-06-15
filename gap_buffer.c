/*
 * Copyright (C) 2015 Richard Burke
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

/* For memrchr */
#define _GNU_SOURCE
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gap_buffer.h"

static void gb_move_gap_to_point(GapBuffer *);
static int gb_increase_gap_if_required(GapBuffer *, size_t);
static int gb_decrease_gap_if_required(GapBuffer *);
static size_t gb_internal_point(const GapBuffer *, size_t);
static size_t gb_external_point(const GapBuffer *, size_t);

GapBuffer *gb_new(size_t size)
{
    assert(size > 0);

    GapBuffer *buffer = malloc(sizeof(GapBuffer));

    if (buffer == NULL) {
        return NULL;
    }

    memset(buffer, 0, sizeof(GapBuffer));

    buffer->text = malloc(size);

    if (buffer->text == NULL) {
        free(buffer);
        return NULL;
    }

    buffer->allocated = size;
    buffer->gap_end = size;

    return buffer;
}

void gb_free(GapBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    free(buffer->text);
    free(buffer);
}

size_t gb_length(const GapBuffer *buffer)
{
    return buffer->allocated - gb_gap_size(buffer);
}

size_t gb_lines(const GapBuffer *buffer)
{
    return buffer->lines;
}

static void gb_move_gap_to_point(GapBuffer *buffer)
{
    if (buffer->point == buffer->gap_start) {
        return;
    } else if (gb_gap_size(buffer) == 0) {
        buffer->gap_start = buffer->gap_end = buffer->point;
    } else if (buffer->point < buffer->gap_start) {
        /* | T | e | s | t |   |   | */
        /* 0   1   2   3   4   5   6 */
        /*     P          GS      GE */

        /* | T |   |   | e | s | t | */
        /* 0   1   2   3   4   5   6 */
        /*    PGS      GE            */
        
        size_t byte_num = buffer->gap_start - buffer->point;

        memmove(buffer->text + buffer->point + buffer->gap_end - buffer->gap_start,
                buffer->text + buffer->point, byte_num);

        buffer->gap_end -= byte_num;
        buffer->gap_start = buffer->point;
    } else {
        /* | T |   |   | e | s | t | */
        /* 0   1   2   3   4   5   6 */
        /*    GS      GE       P     */

        /* | T | e | s |   |   | t | */
        /* 0   1   2   3   4   5   6 */
        /*            PGS      GE    */

        size_t byte_num = buffer->point - buffer->gap_end;

        memmove(buffer->text + buffer->gap_start, buffer->text + buffer->gap_end,
                byte_num);

        buffer->gap_start += byte_num;
        buffer->gap_end += byte_num;
        buffer->point = buffer->gap_start;
    } 
}

size_t gb_gap_size(const GapBuffer *buffer)
{
    return buffer->gap_end - buffer->gap_start;
}

int gb_preallocate(GapBuffer *buffer, size_t size)
{
    return gb_increase_gap_if_required(buffer, size);
}

void gb_contiguous_storage(GapBuffer *buffer)
{
    gb_set_point(buffer, gb_length(buffer));
    gb_move_gap_to_point(buffer);
}

static int gb_increase_gap_if_required(GapBuffer *buffer, size_t new_size)
{
    size_t new_alloc;

    if (new_size > buffer->allocated) {
        new_alloc = new_size + GAP_INCREMENT; 
    } else {
        return 1;
    }

    void *ptr = realloc(buffer->text, new_alloc);

    if (ptr == NULL) {
        return 0;
    }

    buffer->text = ptr;

    size_t byte_num = buffer->allocated - buffer->gap_end;

    if (byte_num > 0) {
        memmove(buffer->text + new_alloc - byte_num, buffer->text + buffer->gap_end,
                byte_num);
    }

    size_t size_increase = new_alloc - buffer->allocated;

    if (buffer->point > buffer->gap_end) {
        buffer->point += size_increase;
    }

    buffer->gap_end += size_increase;
    buffer->allocated = new_alloc;

    /* | T |   |   | e | s | t |   |   |   | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*    GS      GE       P                 */

    /* | T |   |   |   |   |   | e | s | t | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*    GS                  GE       P     */


    /* | T | e | e | e | s | t |   |   |   | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*            PGSE                       */

    /* | T | e | e |   |   |   | e | s | t | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*            PGS         GE             */

    return 1;
}

static int gb_decrease_gap_if_required(GapBuffer *buffer)
{
    if (!(gb_gap_size(buffer) > (2 * GAP_INCREMENT))) {
        return 1;
    }

    size_t buffer_len = gb_length(buffer);
    size_t new_alloc = buffer_len + GAP_INCREMENT;

    size_t point = gb_get_point(buffer);
    gb_set_point(buffer, buffer_len);

    gb_move_gap_to_point(buffer);

    void *ptr = realloc(buffer->text, new_alloc);

    gb_set_point(buffer, point);

    if (ptr == NULL) {
        return 0;
    }

    buffer->text = ptr;
    buffer->gap_end = buffer->gap_start + GAP_INCREMENT;
    buffer->allocated = new_alloc;

    return 1;
}

int gb_insert(GapBuffer *buffer, const char *str, size_t str_len)
{
    assert(str != NULL);

    if (str == NULL) {
        return 0;
    } else if (str_len == 0) {
        return 1;
    }

    gb_move_gap_to_point(buffer);

    size_t new_length = gb_length(buffer) + str_len;

    if (!gb_increase_gap_if_required(buffer, new_length)) {
        return 0;
    }

    char *text = buffer->text + buffer->point;

    for (size_t k = 0; k < str_len; k++) {
        if (*str == '\n') {
            buffer->lines++;
        }

        *text++ = *str++;
    }

    buffer->gap_start += str_len;
    
    return 1;
}

int gb_add(GapBuffer *buffer, const char *str, size_t str_len)
{
    if (!gb_insert(buffer, str, str_len)) {
        return 0;
    }

    buffer->point += str_len;
    return 1;
}

int gb_delete(GapBuffer *buffer, size_t byte_num)
{
    if (byte_num == 0) {
        return 1;
    }

    gb_move_gap_to_point(buffer);

    if (buffer->gap_end + byte_num > buffer->allocated) {
        byte_num = buffer->allocated - buffer->gap_end;
    }

    char *text = buffer->text + buffer->gap_end;

    for (size_t k = 0; k < byte_num; k++) {
        if (*text++ == '\n') {
            buffer->lines--;
        }
    }

    buffer->gap_end += byte_num;

    gb_decrease_gap_if_required(buffer);

    return 1;
}

int gb_replace(GapBuffer *buffer, size_t num_bytes, const char *str, size_t str_len)
{
    /* | T | e | s | t |   |   |   |   |   | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*                PGS                 GE */

    /* | T | e |   |   |   |   |   | s | t | */
    /* 0   1   2   3   4   5   6   7   8   9 */
    /*        PGS                 GE         */

    assert(str != NULL);

    if (str == NULL) {
        return 0;
    }

    gb_move_gap_to_point(buffer);

    size_t buffer_len = gb_length(buffer);

    if (buffer->point + num_bytes > buffer_len) {
        num_bytes = buffer_len - buffer->point;
    }

    size_t after_gap_bytes = buffer->allocated - buffer->gap_end;
    size_t replace_bytes = MIN(after_gap_bytes, MIN(num_bytes, str_len));
    char *text = buffer->text + buffer->gap_end;

    for (size_t k = 0; k < replace_bytes; k++) {
        if (*text == '\n') {
            buffer->lines--; 
        } 

        if (str[k] == '\n') {
            buffer->lines++;
        }

        *text++ = str[k];
    }

    if (replace_bytes > 0) {
        buffer->point += gb_gap_size(buffer) + replace_bytes;
    }

    if (str_len - replace_bytes > 0 && 
       !gb_add(buffer, str + replace_bytes, str_len - replace_bytes)) {
        return 0;
    }

    if (num_bytes > str_len) {
        return gb_delete(buffer, num_bytes - str_len);
    }

    return 1;
}

size_t gb_get_point(const GapBuffer *buffer)
{
    if (buffer->point > buffer->gap_end) {
        return buffer->point - gb_gap_size(buffer);
    }

    return buffer->point;
}

int gb_set_point(GapBuffer *buffer, size_t point)
{
    assert(point <= gb_length(buffer));

    if (point > gb_length(buffer)) {
        return 0;
    } 

    buffer->point = gb_internal_point(buffer, point);

    return 1; 
}

char gb_get(const GapBuffer *buffer)
{
    return gb_get_at(buffer, gb_get_point(buffer));
}

char gb_get_at(const GapBuffer *buffer, size_t point)
{
    assert(point <= gb_length(buffer));

    if (point >= gb_length(buffer)) {
        return '\0';
    } 

    point = gb_internal_point(buffer, point);

    if (point == buffer->gap_start) {
        return *(buffer->text + buffer->gap_end);
    }

    return *(buffer->text + point); 
}

unsigned char gb_getu_at(const GapBuffer *buffer, size_t point)
{
    char c = gb_get_at(buffer, point);
    return *(unsigned char *)&c;
}

size_t gb_get_range(const GapBuffer *buffer, size_t point, char *buf, size_t num_bytes)
{
    size_t buffer_len = gb_length(buffer);
    assert(buf != NULL);
    assert(point <= buffer_len);

    if (buf == NULL || point >= buffer_len || num_bytes == 0) {
        return 0; 
    }

    if (point + num_bytes > buffer_len) {
        num_bytes = buffer_len - point;
    } 

    size_t end = point + num_bytes;

    point = gb_internal_point(buffer, point);
    end = gb_internal_point(buffer, end);

    if (end <= buffer->gap_start || point >= buffer->gap_end) {
        memcpy(buf, buffer->text + point, num_bytes); 
    } else {
        size_t pre_gap_bytes = buffer->gap_start - point;

        if (pre_gap_bytes > 0) {
            memcpy(buf, buffer->text + point, pre_gap_bytes); 
        }

        memcpy(buf + pre_gap_bytes, buffer->text + buffer->gap_end, end - buffer->gap_end); 
    }

    return num_bytes;
}

static size_t gb_internal_point(const GapBuffer *buffer, size_t external_point)
{
    if (external_point > buffer->gap_start) {
        external_point += gb_gap_size(buffer);
    }

    return external_point;
}

static size_t gb_external_point(const GapBuffer *buffer, size_t internal_point)
{
    if (internal_point == buffer->gap_end) {
        return buffer->gap_start; 
    } else if (internal_point > buffer->gap_end) {
        return internal_point - gb_gap_size(buffer);
    }

    return internal_point;
}

int gb_find_next(const GapBuffer *buffer, size_t point, size_t *next, char c)
{
    assert(next != NULL);
    assert(point <= gb_length(buffer));

    if (next == NULL || point >= gb_length(buffer)) {
        return 0;
    }

    point = gb_internal_point(buffer, point);
    char *match;
    size_t offset;

    if (point < buffer->gap_start) {
        match = memchr(buffer->text + point, c, 
                       buffer->gap_start - point);       

        if (match != NULL) {
            offset = match - (buffer->text + point);
            *next = gb_external_point(buffer, point + offset);
            return 1;
        }
    } 
    
    if (point <= buffer->gap_start) {
        point = buffer->gap_end;
    }

    match = memchr(buffer->text + point, c, 
                   buffer->allocated - point);       

    if (match != NULL) {
        offset = match - (buffer->text + point);
        *next = gb_external_point(buffer, point + offset);
        return 1;
    }

    return 0;
}

int gb_find_prev(const GapBuffer *buffer, size_t point, size_t *prev, char c)
{
    size_t buffer_len = gb_length(buffer);
    assert(prev != NULL);
    assert(point <= buffer_len);

    if (prev == NULL || point == 0 || buffer_len == 0) {
        return 0;
    }

    if (point > buffer_len) {
        point = buffer_len;
    }

    point = gb_internal_point(buffer, point);
    char *match;
    size_t offset;

    if (point > buffer->gap_end) {
        match = memrchr(buffer->text + buffer->gap_end, c,
                        point - buffer->gap_end);

        if (match != NULL) {
            offset = match - (buffer->text + buffer->gap_end);
            *prev = gb_external_point(buffer, buffer->gap_end + offset);
            return 1;
        }
    }

    if (point >= buffer->gap_end) {
        point = buffer->gap_start;
    }

    match = memrchr(buffer->text, c, point);

    if (match != NULL) {
        offset = match - buffer->text;
        *prev = gb_external_point(buffer, offset);
        return 1;
    }

    return 0;
}
