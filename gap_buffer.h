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

#ifndef WED_GAP_BUFFER_H
#define WED_GAP_BUFFER_H

#include <stddef.h>

#ifndef GAP_INCREMENT
#define GAP_INCREMENT 1024
#endif

typedef struct {
    char *text;
    size_t point;
    size_t gap_start;
    size_t gap_end;
    size_t allocated;
    size_t lines;
} GapBuffer;

GapBuffer *gb_new(size_t);
void gb_free(GapBuffer *);
size_t gb_length(const GapBuffer *);
size_t gb_lines(const GapBuffer *);
size_t gb_gap_size(const GapBuffer *);
int gb_preallocate(GapBuffer *, size_t);
int gb_insert(GapBuffer *, const char *, size_t);
int gb_add(GapBuffer *, const char *, size_t);
int gb_delete(GapBuffer *, size_t);
size_t gb_get_point(const GapBuffer *);
int gb_set_point(GapBuffer *, size_t);
char gb_get(const GapBuffer *);
char gb_get_at(const GapBuffer *, size_t);
unsigned char gb_getu_at(const GapBuffer *, size_t);
size_t gb_get_range(const GapBuffer *, size_t, char *, size_t);
int gb_find_next(const GapBuffer *, size_t, size_t *, char);
int gb_find_prev(const GapBuffer *, size_t, size_t *, char);

#endif
