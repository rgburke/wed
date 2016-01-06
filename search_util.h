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

#ifndef WED_SEARCH_OPTIONS_H
#define WED_SEARCH_OPTIONS_H

#include <stddef.h>
#include "buffer_pos.h"

/* Base search options common to text and regex search. These
 * values can be set and toggled by the user */
typedef struct {
    char *pattern; /* Text or regex pattern */
    size_t pattern_len; /* Pattern length */
    int case_insensitive; /* True if search should be case insensitive */
    int forward; /* True: forwards, False: backwards */
} SearchOptions;

/* Convenience struct containing various variables used when performing
 * a search */
typedef struct {
    const BufferPos *search_start_pos; /* Position search started from */
    const BufferPos *current_start_pos; /* Current position in buffer */
    int *found_match; /* Set to true if match found */
    size_t *match_point; /* Set to buffer offset of match */
    int *wrapped; /* Set to true when search wraps around the start or end of
                     the buffer */
} SearchData;

typedef enum {
    ES_NONE,
    ES_NEW_LINE,
    ES_TAB,
    ES_HEX_NUMBER,
    ES_BACKSLASH
} EscapeSequence;

/* Each escape sequence has some basic data stored in the struct below
 * which allows them to be handled in a generic way */
typedef struct {
    size_t escape_sequence_length; /* Length of escape sequence text
                                      representation */
    size_t byte_representation_length; /* Length of byte representation */
} EscapeSequenceInfo;

EscapeSequence su_determine_escape_sequence(const char *str, size_t str_len);
EscapeSequenceInfo su_get_escape_sequence_info(EscapeSequence,
                                               int win_line_endings);
char *su_process_string(const char *str, size_t str_len,
                        int win_line_endings, size_t *new_str_len_ptr);

#endif

