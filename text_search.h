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

/* TODO Currently the text search functionality is only guaranteed to work for
 * ASCII text. A comparison is performed byte by byte
 * rather than character by character. This means that a UTF-8 text search
 * will only match if the buffer text and the search text happen to be
 * normalised using the same form. This also means that the current
 * case sensitivity functionality only works for ASCII characters */

#define ALPHABET_SIZE 256

/* Text search struct.
 * The Boyer–Moore–Horspool algorithm is used to perform the search. */
typedef struct {
    char *pattern; /* Text searched for */
    size_t pattern_len; /* Search text length */
    size_t bad_char_table[ALPHABET_SIZE]; /* Array populated with pattern
                                             shift lengths for each character
                                             in the alphabet */
} TextSearch;

Status ts_init(TextSearch *, const SearchOptions *);
Status ts_reinit(TextSearch *, const SearchOptions *);
void ts_free(TextSearch *);
Status ts_find_next(TextSearch *, const SearchOptions *,
                    SearchData *);
Status ts_find_prev(TextSearch *, const SearchOptions *,
                    SearchData *);

#endif
