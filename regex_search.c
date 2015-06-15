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

#define REGEX_BUFFER_SIZE 8192

#include <string.h>
#include <assert.h>
#include "regex_search.h"
#include "status.h"
#include "buffer_pos.h"
#include "util.h"

static Status rs_find_prev_str(const char *, size_t, size_t, size_t, size_t *, int *, RegexSearch *);
static Status rs_find_next_str(const char *, size_t, size_t, size_t *, int *, RegexSearch *);

Status rs_init(RegexSearch *search, const SearchOptions *opt)
{
    assert(search != NULL);
    assert(opt != NULL);
    assert(opt->pattern_len > 0);
    assert(!is_null_or_empty(opt->pattern));

    memset(search, 0, sizeof(RegexSearch));

    int options = PCRE_MULTILINE | PCRE_UTF8;

    if (opt->case_insensitive) {
        options |= PCRE_CASELESS;
    }

    const char *error_str;
    int error_offset;

    search->regex = pcre_compile(opt->pattern, options, &error_str, &error_offset, NULL);

    if (search->regex == NULL) {
        return st_get_error(ERR_INVALID_REGEX, "Invalid regex - %s - at position %d", error_str, error_offset);         
    }

    search->study = pcre_study(search->regex, 0, &error_str);

    return STATUS_SUCCESS;
}

void rs_free(RegexSearch *search)
{
    if (search == NULL) {
        return;
    }

    pcre_free_study(search->study);
    pcre_free(search->regex);
}

Status rs_reinit(RegexSearch *search, const SearchOptions *opt)
{
    rs_free(search);
    return rs_init(search, opt);
}

Status rs_find_next(RegexSearch *search, const SearchOptions *opt,
                    const BufferPos *search_start_pos, const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point)
{
    BufferPos pos = *current_start_pos;
    int wrapped = search_start_pos != NULL && bp_compare(search_start_pos, current_start_pos) == 1;
    size_t buffer_len = gb_length(pos.data);
    size_t limit;
    (void)opt;

    gb_contiguous_storage((GapBuffer *)pos.data);

    if (wrapped) {
        limit = MIN(search_start_pos->offset + REGEX_BUFFER_SIZE, buffer_len);
    } else {
        limit = buffer_len;
    }

    RETURN_IF_FAIL(rs_find_next_str(pos.data->text, pos.offset, limit, 
                                    match_point, found_match, search));

    if (*found_match || wrapped) {
        return STATUS_SUCCESS;
    }

    bp_to_buffer_start(&pos);

    if (search_start_pos == NULL) {
        limit = current_start_pos->offset;
    } else {
        limit = search_start_pos->offset;
    }

    RETURN_IF_FAIL(rs_find_next_str(pos.data->text, pos.offset, 
                                    MIN(limit + REGEX_BUFFER_SIZE, buffer_len), 
                                    match_point, found_match, search));

    return STATUS_SUCCESS;
}

Status rs_find_prev(RegexSearch *search, const SearchOptions *opt,
                    const BufferPos *search_start_pos, const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point)
{
    BufferPos pos = *current_start_pos;
    size_t buffer_len = gb_length(pos.data);
    int wrapped = search_start_pos != NULL && bp_compare(search_start_pos, current_start_pos) == -1;
    size_t limit;
    (void)opt;

    if (wrapped) {
        limit = search_start_pos->offset;
    } else {
        limit = 0;
    }

    gb_contiguous_storage((GapBuffer *)pos.data);

    RETURN_IF_FAIL(rs_find_prev_str(pos.data->text, buffer_len, pos.offset, 
                                    limit, match_point, found_match, search));

    if (*found_match || wrapped) {
        if (*found_match && wrapped && 
            *match_point < search_start_pos->offset) {
            *found_match = 0;
        }

        return STATUS_SUCCESS;
    }

    if (search_start_pos == NULL) {
        limit = current_start_pos->offset;
    } else {
        limit = search_start_pos->offset;
    }

    RETURN_IF_FAIL(rs_find_prev_str(pos.data->text, buffer_len, buffer_len, 
                                    limit, match_point, found_match, search));

    return STATUS_SUCCESS;
}

static Status rs_find_prev_str(const char *str, size_t str_len, size_t point, size_t limit, 
                               size_t *match_point, int *found_match, RegexSearch *search)
{
    size_t search_length, search_point;
    size_t mpoint = 0, mlength = 0;
    size_t start_point = point;
    Status status = STATUS_SUCCESS;
    int found = 0;

    while (point > limit) {
        search_length = MIN(point - limit, REGEX_BUFFER_SIZE);
        point -= search_length;
        search_length = MIN(search_length + REGEX_BUFFER_SIZE, str_len - point);
        search_point = point;

        do {
            found = 0;

            status = rs_find_next_str(str, search_point, point + search_length, 
                                      match_point, &found, search);

            if (found && *match_point < start_point) {
                *found_match = 1;
                mpoint = *match_point;
                mlength = search->match_length;
                search_point = *match_point + search->match_length;
            } else {
                break;
            }
        } while (STATUS_IS_SUCCESS(status) && search_point < start_point);

        if (*found_match || !STATUS_IS_SUCCESS(status)) {
            if (*found_match && (mpoint != *match_point || search->return_code < 1)) {
                status = rs_find_next_str(str, mpoint, mpoint + mlength, 
                                          match_point, found_match, search);
            }

            return status;
        }
    }

    return status;
}

static Status rs_find_next_str(const char *str, size_t point, size_t limit, 
                               size_t *match_point, int *found_match,
                               RegexSearch *search)
{
    search->return_code = pcre_exec(search->regex, search->study, str, limit, point, 
                                    0, search->output_vector, OUTPUT_VECTOR_SIZE);

    if (search->return_code < 0) {
        if (search->return_code == PCRE_ERROR_NOMATCH) {
            return STATUS_SUCCESS;
        }

        return st_get_error(ERR_REGEX_EXECUTION_FAILED, 
                            "Regex execution failed. PCRE exit code: %d", 
                            search->return_code);
    }

    *match_point = search->output_vector[0];
    search->match_length = search->output_vector[1] - search->output_vector[0];
    *found_match = 1;

    return STATUS_SUCCESS;
}

