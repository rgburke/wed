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

#ifndef WED_TEXT_SEARCH_H
#define WED_TEXT_SEARCH_H

#include "shared.h"
#include "buffer_pos.h"
#include "search_options.h"
#include "status.h"

#define ALPHABET_SIZE 256

typedef struct {
    char *pattern;
    size_t pattern_len;
    size_t bad_char_table[ALPHABET_SIZE];
} TextSearch;

Status ts_init(TextSearch *, const SearchOptions *);
Status ts_reinit(TextSearch *, const SearchOptions *);
void ts_free(TextSearch *);
Status ts_find_next(TextSearch *, const SearchOptions *, const BufferPos *, int *, size_t *);
Status ts_find_prev(TextSearch *, const SearchOptions *, const BufferPos *, int *, size_t *);

#endif
