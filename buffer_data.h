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

#ifndef WED_BUFFER_DATA_H
#define WED_BUFFER_DATA_H

#include "buffer_segment.h"

typedef struct {
    BufferSegment *seg;
    size_t length;
    size_t point;
} BufferData;

BufferData *bd_new_sized(size_t);
BufferData *bd_new(void);
void bd_free(BufferData *);
int bd_insert(BufferData *, const char *, size_t);
int bd_delete(BufferData *, size_t);
size_t bd_get_point(BufferData *);
int bd_set_point(BufferData *, size_t);
char bd_get_at(BufferData *, size_t);

#endif
