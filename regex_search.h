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

#ifndef WED_REGEX_SEARCH_H
#define WED_REGEX_SEARCH_H

#include <pcre.h>
#include "status.h"
#include "buffer_pos.h"
#include "search_options.h"

#define OUTPUT_VECTOR_SIZE 90
#define MAX_CAPTURE_GROUP_NUM (((OUTPUT_VECTOR_SIZE - (OUTPUT_VECTOR_SIZE / 3)) / 2) - 1)
#define MAX_BACK_REF_OCCURRENCES 100

typedef struct {
    size_t back_refs[MAX_BACK_REF_OCCURRENCES][3];
    size_t back_ref_occurrences;
} RegexReplace;

typedef struct {
    pcre *regex;
    pcre_extra *study;
    int return_code;
    int output_vector[OUTPUT_VECTOR_SIZE];
    int match_length;
    RegexReplace regex_replace;
} RegexSearch;

Status rs_init(RegexSearch *, const SearchOptions *);
void rs_free(RegexSearch *);
Status rs_reinit(RegexSearch *, const SearchOptions *);
Status rs_find_next(RegexSearch *, const SearchOptions *, const BufferPos *, const BufferPos *, int *, size_t *);
Status rs_find_prev(RegexSearch *, const SearchOptions *, const BufferPos *, const BufferPos *, int *, size_t *);

#endif
