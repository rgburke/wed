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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "text_search.h"
#include "shared.h"
#include "util.h"

#define SEARCH_BUFFER_SIZE 8192

static size_t ts_gb_internal_point(const GapBuffer *, size_t external_point);
static size_t ts_gb_external_point(const GapBuffer *, size_t internal_point);
static int ts_find_prev_str(const GapBuffer *, size_t point, size_t *prev,
                            size_t limit, const TextSearch *);
static int ts_find_next_str(const GapBuffer *, size_t point, size_t *next,
                            size_t limit, const TextSearch *);
static int ts_find_next_str_in_range(const char *text, size_t *start_point,
                                     size_t limit, size_t *next,
                                     const TextSearch *);
static void ts_populate_bad_char_table(size_t bad_char_table[ALPHABET_SIZE],
                                       const char *pattern, size_t pattern_len);
static void ts_update_search_chars(int case_insensitive);

static int ts_search_chars_lc = 0;

static uchar ts_search_chars[ALPHABET_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

Status ts_init(TextSearch *search, const SearchOptions *opt)
{
    assert(search != NULL);
    assert(opt != NULL);
    assert(opt->pattern_len > 0);
    assert(opt->pattern != NULL);

    search->pattern = malloc(opt->pattern_len + 1);

    if (search->pattern == NULL) {
        return OUT_OF_MEMORY("Unable to copy pattern");
    }

    memcpy(search->pattern, opt->pattern, opt->pattern_len);
    search->pattern[opt->pattern_len] = '\0';

    search->pattern_len = opt->pattern_len;

    if (opt->case_insensitive) {
        ts_update_search_chars(opt->case_insensitive);
        uchar *pat = (uchar *)search->pattern;

        /* Convert ASCII characters in pattern to lower case */
        for (size_t k = 0; k < opt->pattern_len; k++) {
            pat[k] = ts_search_chars[pat[k]];
        }
    }

    ts_populate_bad_char_table(search->bad_char_table, search->pattern,
                               search->pattern_len);

    return STATUS_SUCCESS;
}

Status ts_reinit(TextSearch *search, const SearchOptions *opt)
{
    ts_free(search);
    return ts_init(search, opt);
}

void ts_free(TextSearch *search)
{
    free(search->pattern);
    search->pattern = NULL;
}

Status ts_find_next(TextSearch *search, const SearchOptions *opt,
                    SearchData *data)
{
    ts_update_search_chars(opt->case_insensitive);

    BufferPos pos = *data->current_start_pos;
    size_t limit;

    if (*data->wrapped) {
        /* Add opt->pattern_len - 1 to the search limit here in 
         * case the search start position was in the middle of a match */
        limit = data->search_start_pos->offset + opt->pattern_len - 1;
    } else {
        limit = gb_length(pos.data);
    }

    if (ts_find_next_str(pos.data, pos.offset, data->match_point,
                         limit, search)) {
        *data->found_match = 1;
        return STATUS_SUCCESS;
    }

    if (*data->wrapped) {
        /* Entire buffer has been searched by this point so return */
        return STATUS_SUCCESS;
    } else if (data->search_start_pos != NULL) {
        *data->wrapped = 1;
    }

    bp_to_buffer_start(&pos);
   
    if (data->search_start_pos == NULL) {
        limit = data->current_start_pos->offset;
    } else {
        limit = data->search_start_pos->offset;
    }

    if (ts_find_next_str(pos.data, pos.offset, data->match_point,
                         limit + opt->pattern_len - 1, search)) {
        *data->found_match = 1;
    }

    return STATUS_SUCCESS;
}

Status ts_find_prev(TextSearch *search, const SearchOptions *opt,
                    SearchData *data)
{
    ts_update_search_chars(opt->case_insensitive);

    BufferPos pos = *data->current_start_pos;
    size_t limit;

    if (*data->wrapped) {
        limit = data->search_start_pos->offset;
    } else {
        limit = 0;
    }

    if (ts_find_prev_str(pos.data, pos.offset, data->match_point,
                         limit, search)) {
        *data->found_match = 1;
        return STATUS_SUCCESS;
    }

    if (*data->wrapped) {
        return STATUS_SUCCESS;
    } else if (data->search_start_pos != NULL) {
        *data->wrapped = 1;
    }

    bp_to_buffer_end(&pos);

    if (data->search_start_pos == NULL) {
        limit = data->current_start_pos->offset;
    } else {
        limit = data->search_start_pos->offset;
    }

    if (ts_find_prev_str(pos.data, pos.offset, data->match_point,
                         limit, search)) {
        *data->found_match = 1;
    }

    return STATUS_SUCCESS;
}

/* Perform a reverse search by splitting the buffer into chunks
 * of size SEARCH_BUFFER_SIZE (or remaining space) and searching
 * forwards in each chunk */
static int ts_find_prev_str(const GapBuffer *buffer, size_t point,
                            size_t *prev, size_t limit,
                            const TextSearch *search)
{
    size_t search_length, search_point;
    size_t buffer_len = gb_length(buffer);
    int found = 0;

    while (point > limit) {
        search_length = MIN(point - limit, SEARCH_BUFFER_SIZE);
        point -= search_length;
        search_length = MIN(search_length + search->pattern_len - 1,
                            buffer_len - point);
        search_point = point;

        while (ts_find_next_str(buffer, search_point, prev,
                                point + search_length, search)) {
            found = 1;
            search_point = *prev + 1;
        }

        if (found) {
            return 1;
        }
    }

    return 0;
}

static size_t ts_gb_internal_point(const GapBuffer *buffer,
                                   size_t external_point)
{
    if (external_point > buffer->gap_start) {
        external_point += gb_gap_size(buffer);
    }

    return external_point;
}

static size_t ts_gb_external_point(const GapBuffer *buffer,
                                   size_t internal_point)
{
    if (internal_point == buffer->gap_end) {
        return buffer->gap_start; 
    } else if (internal_point > buffer->gap_end) {
        return internal_point - gb_gap_size(buffer);
    }

    return internal_point;
}

/* This function works around the gap to determine the searches that
 * need to be performed. Although this adds complexity it allows a 
 * search to be performed without moving the gap */
static int ts_find_next_str(const GapBuffer *buffer, size_t point,
                            size_t *next, size_t limit,
                            const TextSearch *search)
{
    size_t buffer_len = gb_length(buffer);

    if (next == NULL || point >= buffer_len ||
        limit < point + search->pattern_len ||
        search->pattern_len == 0 ||
        point + search->pattern_len > buffer_len) {
        return 0;
    }

    if (limit > buffer_len) {
        limit = buffer_len;
    }

    size_t limit_ext = limit;
    point = ts_gb_internal_point(buffer, point);
    limit = ts_gb_internal_point(buffer, limit);

    if (point + search->pattern_len <= buffer->gap_start) {
        if (ts_find_next_str_in_range(buffer->text, &point,
                                      MIN(limit, buffer->gap_start),
                                      next, search)) {
            return 1;
        }
    }

    if (point + search->pattern_len > limit || limit < buffer->gap_start) {
        return 0;
    }

    if (point < buffer->gap_start) {
        /* We need to search text that is separated by the gap. To do this
         * we create a temporary buffer that joins the separated text
         * together. The amount of text we need to join together is limited
         * by our distance from the gap as well as the length of the 
         * pattern searched for i.e. This will be a relatively small
         * amount of text */
        size_t gap_bridge_size = MIN(buffer->gap_start -
                                     point + search->pattern_len, buffer_len);
        /* TODO Still need to limit the search text length
         * to avoid stack overflow */
        char gap_bridge[gap_bridge_size];
        size_t copied = gb_get_range(buffer, point, gap_bridge,
                                     gap_bridge_size);

        if (copied != gap_bridge_size) {
            return 0;
        }

        size_t bridge_point = 0;
        size_t bridge_limit = MIN(limit_ext - point, gap_bridge_size);
        
        if (ts_find_next_str_in_range(gap_bridge, &bridge_point, bridge_limit,
                                      next, search)) {
            *next += point;
            return 1;            
        }

        point = buffer->gap_end;
    } else if (point == buffer->gap_start) {
        point = buffer->gap_end;
    }

    if (point + search->pattern_len > limit) {
        return 0;
    }

    if (point + search->pattern_len <= buffer->allocated) {
        if (ts_find_next_str_in_range(buffer->text, &point,
                                      MIN(limit, buffer->allocated),
                                      next, search)) {
            *next = ts_gb_external_point(buffer, *next);
            return 1;
        }
    }

    return 0;
}

/* Search string using Boyer–Moore–Horspool algorithm */
static int ts_find_next_str_in_range(const char *text, size_t *start_point,
                                     size_t limit, size_t *next,
                                     const TextSearch *search)
{
    const uchar *txt = (const uchar *)text;
    const uchar *pattern = (const uchar *)search->pattern;
    size_t point = *start_point + search->pattern_len - 1;
    size_t sub_start_point = point;
    size_t pattern_idx;

    while (point < limit) {
        pattern_idx = search->pattern_len;
        sub_start_point = point;

        while (pattern_idx != 0 && 
               ts_search_chars[*(txt + point)] == pattern[pattern_idx - 1]) {
            pattern_idx--;
            point--;
        }

        if (pattern_idx == 0) {
            *next = sub_start_point - (search->pattern_len - 1);
            return 1;
        } else {
            /* Shift pattern */
            point = sub_start_point +
                    search->bad_char_table[
                        ts_search_chars[*(txt + sub_start_point)]
                    ];
        }
    }

    /* Keep track of where we've searched up to */
    *start_point = sub_start_point;

    return 0;
}

static void ts_populate_bad_char_table(size_t bad_char_table[ALPHABET_SIZE],
                                       const char *pattern, size_t pattern_len)
{
    for (size_t k = 0; k < ALPHABET_SIZE; k++) {
        bad_char_table[k] = pattern_len; 
    }

    const uchar *pat = (const uchar *)pattern;

    /* For those characters in the alphabet which appear
     * in the pattern calculate the correct shift length */
    for (size_t k = 0; k < pattern_len - 1; k++) {
        bad_char_table[pat[k]] = pattern_len - 1 - k;
    }
}

/* If case insensitive set the relevant ASCII chars to their
 * lower case equivalents otherwise set them back to their
 * original values */
static void ts_update_search_chars(int case_insensitive)
{
    if (case_insensitive && !ts_search_chars_lc) {
        for (int k = 'A'; k <= 'Z'; k++) {
            ts_search_chars[k] += 32;
        }

        ts_search_chars_lc = 1;
    } else if (!case_insensitive && ts_search_chars_lc) {
        for (int k = 'A'; k <= 'Z'; k++) {
            ts_search_chars[k] -= 32;
        }

        ts_search_chars_lc = 0;
    }
}
