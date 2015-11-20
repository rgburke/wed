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

/* GapBuffer is the data structure used to
 * store text in wed */
typedef struct {
    char *text; /* Memory allocated to hold text */
    size_t point; /* Position in buffer */
    size_t gap_start; /* Position gap starts */
    size_t gap_end; /* Position gap ends */
    size_t allocated; /* Bytes allocated */
    size_t lines; /* Number of new line (\n) characters */
} GapBuffer;

GapBuffer *gb_new(size_t size);
void gb_free(GapBuffer *);
size_t gb_length(const GapBuffer *);
size_t gb_lines(const GapBuffer *);
size_t gb_gap_size(const GapBuffer *);
int gb_preallocate(GapBuffer *, size_t size);
void gb_contiguous_storage(GapBuffer *);
int gb_insert(GapBuffer *, const char *str, size_t str_len);
int gb_add(GapBuffer *, const char *str, size_t str_len);
int gb_delete(GapBuffer *, size_t byte_num);
int gb_replace(GapBuffer *, size_t byte_num, const char *str, size_t str_len);
size_t gb_get_point(const GapBuffer *);
int gb_set_point(GapBuffer *, size_t point);
char gb_get(const GapBuffer *);
char gb_get_at(const GapBuffer *, size_t point);
unsigned char gb_getu_at(const GapBuffer *, size_t point);
size_t gb_get_range(const GapBuffer *, size_t point, char *buf,
                    size_t num_bytes);
int gb_find_next(const GapBuffer *, size_t point, size_t *next, char c);
int gb_find_prev(const GapBuffer *, size_t point, size_t *prev, char c);

#endif
