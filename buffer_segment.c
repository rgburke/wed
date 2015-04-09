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

#include <stdlib.h>
#include <string.h>
#include "buffer_segment.h"

BufferSegment *bs_new(size_t size)
{
    BufferSegment *seg = malloc(sizeof(BufferSegment));

    if (seg == NULL) {
        return NULL;
    }

    memset(seg, 0, sizeof(BufferSegment));

    seg->buffer = gb_new(size);

    if (seg->buffer == NULL) {
        free(seg);
        return NULL;
    }

    return seg;
}

void bs_free(BufferSegment *seg)
{
    if (seg == NULL) {
        return;
    }

    gb_free(seg->buffer);
    free(seg);
}

int bs_split(BufferDataPos data_pos)
{
    size_t buffer_lenth = gb_length(data_pos.seg->buffer);
    size_t new_seg_size = buffer_lenth - data_pos.point;

    BufferSegment *seg = bs_new(new_seg_size + GAP_INCREMENT);

    if (seg == NULL) {
        return 0;
    }

    char buf[1024];
    size_t bytes_copied;

    for (size_t k = data_pos.point; k < buffer_lenth; k += bytes_copied) {
        bytes_copied = gb_get_range(data_pos.seg->buffer, k, buf, sizeof(buf));
        gb_add(seg->buffer, buf, bytes_copied);
    }

    gb_set_point(data_pos.seg->buffer, data_pos.point);
    gb_delete(data_pos.seg->buffer, new_seg_size);

    BufferSegment *next = data_pos.seg->next;

    data_pos.seg->next = seg;
    seg->prev = data_pos.seg;

    if (next != NULL) {
        next->prev = seg;
        seg->next = next;
    }

    return 1;
}

int bs_insert(BufferDataPos data_pos, const char *str, size_t str_len, size_t *bytes_inserted)
{
    *bytes_inserted = 0;

    if (str_len == 0) {
        return 1;
    }

    size_t bytes_remaining = MAX_SEGMENT_SIZE - gb_length(data_pos.seg->buffer);

    if (bytes_remaining == 0) {
        return 1;
    }

    gb_set_point(data_pos.seg->buffer, data_pos.point);
    bytes_remaining = bytes_remaining > str_len ? str_len : bytes_remaining;

    if (!gb_insert(data_pos.seg->buffer, str, bytes_remaining)) {
        return 0;
    }

    *bytes_inserted = bytes_remaining;

    return 1;
}

int bs_delete(BufferDataPos data_pos, size_t byte_num, size_t *bytes_deleted)
{
    size_t bytes_remaining = gb_length(data_pos.seg->buffer) - data_pos.point;
    *bytes_deleted = 0;

    if (byte_num == 0 || bytes_remaining == 0) {
        return 1;
    }

    gb_set_point(data_pos.seg->buffer, data_pos.point);
    byte_num = byte_num > bytes_remaining ? bytes_remaining : byte_num;

    if (!gb_delete(data_pos.seg->buffer, byte_num)) {
        return 0;
    }

    *bytes_deleted = byte_num;

    return 1;
}

size_t bs_length(BufferSegment *seg)
{
    return gb_length(seg->buffer);
}

