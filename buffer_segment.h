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

#ifndef WED_BUFFER_SEGMENT_H
#define WED_BUFFER_SEGMENT_H

#include "gap_buffer.h"

#define MAX_SEGMENT_SIZE (1024 * 1024)
#define NEW_SEGMENT_SIZE (MAX_SEGMENT_SIZE - GAP_INCREMENT)

typedef struct BufferSegment BufferSegment;

struct BufferSegment {
    BufferSegment *next;
    BufferSegment *prev;   
    GapBuffer *buffer;
};

typedef struct {
    BufferSegment *seg;
    size_t point;
} BufferDataPos;

BufferSegment *bs_new(size_t);
void bs_free(BufferSegment *);
int bs_split(BufferDataPos);
int bs_insert(BufferDataPos, const char *, size_t, size_t *);
int bs_delete(BufferDataPos, size_t, size_t *);
size_t bs_length(BufferSegment *);

#endif
