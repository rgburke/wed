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

#ifndef WED_SEARCH_H
#define WED_SEARCH_H

#include "shared.h"
#include "buffer_pos.h"

#define ALPHABET_SIZE 256

typedef struct {
    char *pattern;
    size_t pattern_len;
    BufferPos start_pos;
    BufferPos last_match_pos;
    size_t bad_char_table[ALPHABET_SIZE];
    int case_insensitive;
} BufferSearch;

int bs_init(BufferSearch *, const char *, size_t, const BufferPos *, int);
int bs_reinit(BufferSearch *, const char *, size_t, const BufferPos *, int);
void bs_free(BufferSearch *);
int bs_find_next(BufferSearch *);

#endif
