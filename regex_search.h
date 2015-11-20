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

/* Recommend reading: man pcreapi */

/* pcre_exec returns captured groups using an array of integers.
 * The size of this array must be a multiple of 3 with one third
 * used as workspace and the remaining two thirds containing
 * group data. */

/* Size of output vector used with pcre_exec */
#define OUTPUT_VECTOR_SIZE 90
/* The number of groups that can be captured using an output vector
 * of size OUTPUT_VECTOR_SIZE. This is calculated as follows:
 * Only a third of the array can contain captured group data, data about
 * captured groups is stored using a pair of integers so divide by 2
 * and finally groups start from 0 with group 0 being the entire string
 * matched so minus 1 to get the number of capture groups available to
 * the user. */
#define MAX_CAPTURE_GROUP_NUM \
            (((OUTPUT_VECTOR_SIZE - (OUTPUT_VECTOR_SIZE / 3)) / 2) - 1)
/* The max number of backreferences that can appear in replace text */
#define MAX_BACK_REF_OCCURRENCES 100

/* This structure contains backreference data processed from replace text 
 * entered by the user */
typedef struct {
    /* Each backreference that occurs in replace text has an entry
     * in the back_refs array below. Each entry has the following 3 properties
     * stored in an array:
     *
     * 0: backreference number
     * 1: starting index in replace string
     * 2: backreference length
     *
     * e.g. If the user did a regex find and replace and entered \4 as
     * the replace text then an entry in back_refs would look like:
     *
     * 0: 4
     * 1: 0
     * 2: 2 
     *
     * This information is stored to allow the backreference to be replaced
     * with the actual matched text once the search has been performed. */
    size_t back_refs[MAX_BACK_REF_OCCURRENCES][3];

    /* The number of backreference entries in the back_refs array */
    size_t back_ref_occurrences;
} RegexReplace;

/* Regex search, match and replace data */
typedef struct {
    pcre *regex; /* Compiled PCRE regex */
    pcre_extra *study; /* Extra data PCRE can use to optimize search */
    int return_code; /* pcre_exec return code */
    int output_vector[OUTPUT_VECTOR_SIZE]; /* Captured group data */
    int match_length; /* output_vector[1] - output_vector[0] for convenience */
    RegexReplace regex_replace; /* Backreference data */
} RegexSearch;

Status rs_init(RegexSearch *, const SearchOptions *);
void rs_free(RegexSearch *);
Status rs_reinit(RegexSearch *, const SearchOptions *);
Status rs_find_next(RegexSearch *, const SearchOptions *,
                    const BufferPos *search_start_pos,
                    const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point);
Status rs_find_prev(RegexSearch *, const SearchOptions *,
                    const BufferPos *search_start_pos,
                    const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point);

#endif
