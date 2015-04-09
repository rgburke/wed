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
#include <stdio.h>
#include "buffer_data.h"

static int bd_convert_point_to_pos(BufferData *, size_t, BufferDataPos *);
static void free_segment_if_empty(BufferData *, BufferSegment *);
static void advance_data_pos_segment(BufferDataPos *);

BufferData *bd_new(void)
{
    BufferData *data = malloc(sizeof(BufferData));    

    if (data == NULL) {
        return NULL;
    }

    data->seg = bs_new(GAP_INCREMENT);

    if (data->seg == NULL) {
        free(data);
        return NULL;
    }

    return data;
}

void bd_free(BufferData *data)
{
    if (data == NULL) {
        return;
    }

    BufferSegment *seg = data->seg;
    BufferSegment *tmp;

    while (seg != NULL) {
        tmp = seg->next;
        bs_free(seg);
        seg = tmp;
    }
}

static int bd_convert_point_to_pos(BufferData *data, size_t point, BufferDataPos *data_pos)
{
    memset(data_pos, 0, sizeof(BufferDataPos));

    if (point > data->length) {
        return 0;
    }

    BufferSegment *seg = data->seg;
    size_t length = 0;
    size_t seg_length = 0;

    while (seg != NULL) {
        seg_length = bs_length(seg);

        if (length + seg_length >= point) {
            break;
        }

        length += seg_length;
        seg = seg->next;
    }

    data_pos->seg = seg;
    data_pos->point = point - length;

    /* When inserting data want to use first position found
     * When retrieving data want to use later position */

    return 1;
}

int bd_insert(BufferData *data, const char *str, size_t str_len)
{
    if (str_len == 0) {
        return 1;
    }

    BufferDataPos data_pos;
    bd_convert_point_to_pos(data, data->point, &data_pos);

    size_t bytes_remaining = str_len;
    size_t bytes_inserted;

    if (!bs_insert(data_pos, str, str_len, &bytes_inserted)) {
        return 0;
    }

    bytes_remaining -= bytes_inserted;
    data->length += bytes_inserted;
    data->point += bytes_inserted;
    data_pos.point += bytes_inserted;

    if (bytes_remaining == 0) {
        return 1;
    }

    if (data_pos.point == bs_length(data_pos.seg)) {
        if (data_pos.seg->next != NULL) {
            data_pos = (BufferDataPos) {
                .seg = data_pos.seg->next,
                .point = 0
            };

            if (!bs_insert(data_pos, str + (str_len - bytes_remaining),
                           bytes_remaining, &bytes_inserted)) {
                return 0;
            }

            bytes_remaining -= bytes_inserted;
            data->length += bytes_inserted;
            data->point += bytes_inserted;
        }
    } else if (!bs_split(data_pos)) {
        return 0;
    }

    BufferSegment *prev_seg = data_pos.seg, *new_seg;
    size_t alloc_size;

    while (bytes_remaining > 0) {
        alloc_size = bytes_remaining > NEW_SEGMENT_SIZE ? NEW_SEGMENT_SIZE : bytes_remaining; 
        alloc_size = alloc_size < GAP_INCREMENT ? GAP_INCREMENT : alloc_size;
        new_seg = bs_new(alloc_size);

        if (new_seg == NULL) {
            return 0;
        }

        data_pos = (BufferDataPos) {
            .seg = new_seg,
            .point = 0
        };

        if (!bs_insert(data_pos, str + (str_len - bytes_remaining), alloc_size, &bytes_inserted)) {
            bs_free(new_seg);
            return 0;
        }

        new_seg->next = prev_seg->next;
        new_seg->prev = prev_seg;
        prev_seg->next = new_seg;

        if (new_seg->next != NULL) {
            new_seg->next->prev = new_seg;
        }

        prev_seg = new_seg;

        bytes_remaining -= bytes_inserted;
        data->length += bytes_inserted;
        data->point += bytes_inserted;
    }

    return 1;
}

int bd_delete(BufferData *data, size_t byte_num)
{
    if (byte_num == 0) {
        return 1;
    }

    BufferDataPos data_pos;
    bd_convert_point_to_pos(data, data->point, &data_pos);

    size_t bytes_remaining = data->length - data->point;
    size_t bytes_deleted;

    bytes_remaining = byte_num > bytes_remaining ? bytes_remaining : byte_num; 

    if (!bs_delete(data_pos, bytes_remaining, &bytes_deleted)) {
        return 0;
    }

    bytes_remaining -= bytes_deleted;
    data->length -= bytes_deleted;

    BufferSegment *seg = data_pos.seg->next;

    free_segment_if_empty(data, data_pos.seg);

    while (bytes_remaining > 0) {
        data_pos = (BufferDataPos) {
            .seg = seg,
            .point = 0
        };

        if (!bs_delete(data_pos, bytes_remaining, &bytes_deleted)) {
            return 0;
        }

        bytes_remaining -= bytes_deleted;
        data->length -= bytes_deleted;

        free_segment_if_empty(data, seg);

        seg = seg->next;
    }

    return 1;
}

static void free_segment_if_empty(BufferData *data, BufferSegment *seg)
{
    if (bs_length(seg) == 0 && !(seg->next == NULL && seg->prev == NULL)) {
        if (seg->prev == NULL) {
            data->seg = seg->next;
        } else {
            seg->prev->next = seg->next;
        }

        if (seg->next != NULL) {
            seg->next->prev = seg->prev;
        }

        bs_free(seg);
    }
}

size_t bd_get_point(BufferData *data)
{
    return data->point;
}

int bd_set_point(BufferData *data, size_t point)
{
    if (point > data->length) {
        return 0;
    }

    data->point = point;

    return 1;
}

char bd_get_at(BufferData *data, size_t point)
{
    BufferDataPos data_pos;

    if (!bd_convert_point_to_pos(data, point, &data_pos)) {
        return '\0';
    }

    advance_data_pos_segment(&data_pos);

    return gb_get_at(data_pos.seg->buffer, data_pos.point);
}

static void advance_data_pos_segment(BufferDataPos *data_pos)
{
    if (data_pos->point != bs_length(data_pos->seg)) {
        return;
    }

    int found_seg = 0;
    BufferSegment *seg = data_pos->seg;

    while (seg->next != NULL) {
        seg = seg->next;

        if (bs_length(seg) > 0) {
            found_seg = 1;
            break;
        }
    }

    if (found_seg) {
        data_pos->seg = seg;
        data_pos->point = 0;
    }
}

