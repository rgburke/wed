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

/* Extend search limit by REGEX_BUFFER_SIZE to allow patterns that start
 * before the limit and end after to be matched */
#define REGEX_BUFFER_SIZE 8192

#include <string.h>
#include <assert.h>
#include "regex_search.h"
#include "status.h"
#include "buffer_pos.h"
#include "util.h"

static Status rs_find_prev_str(const char *str, size_t str_len, size_t point,
                               size_t limit, size_t *match_point,
                               int *found_match, RegexSearch *);
static Status rs_find_next_str(const char *str, size_t point, size_t limit,
                               size_t *match_point, int *found_match,
                               RegexSearch *);

/* Initialise regex search */
Status rs_init(RegexSearch *search, const SearchOptions *opt)
{
    assert(search != NULL);
    assert(opt != NULL);
    assert(opt->pattern_len > 0);
    assert(!is_null_or_empty(opt->pattern));

    memset(search, 0, sizeof(RegexSearch));

    /* All user regex searches are multiline (equivalent to
     * Perl's /m option) by default */
    /* TODO need a way of specifying all possible regex flags
     * in prompt when performing a regex search */
    int options = PCRE_MULTILINE | PCRE_UTF8;

    if (opt->case_insensitive) {
        options |= PCRE_CASELESS;
    }

    const char *error_str;
    int error_offset;

    search->regex = pcre_compile(opt->pattern, options, &error_str,
                                 &error_offset, NULL);

    if (search->regex == NULL) {
        return st_get_error(ERR_INVALID_REGEX,
                            "Invalid regex - %s - at position %d",
                            error_str, error_offset);         
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
                    const BufferPos *search_start_pos,
                    const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point)
{
    BufferPos pos = *current_start_pos;
    /* search_start_pos is only set when a find & replace is performed so
     * that an end is defined for the search. For a normal find operation
     * search_start_pos is not set so that the next match in the whole buffer
     * will be found if possible */
    int wrapped = search_start_pos != NULL &&
                  bp_compare(search_start_pos, current_start_pos) == 1;
    size_t buffer_len = gb_length(pos.data);
    size_t limit;
    (void)opt;

    gb_contiguous_storage((GapBuffer *)pos.data);

    if (wrapped) {
        /* Search has wrapped so set the limit to the search starting
         * position (plus buffer) or the remaining length of the buffer */
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
                    const BufferPos *search_start_pos,
                    const BufferPos *current_start_pos,
                    int *found_match, size_t *match_point)
{
    BufferPos pos = *current_start_pos;
    size_t buffer_len = gb_length(pos.data);
    int wrapped = search_start_pos != NULL &&
                  bp_compare(search_start_pos, current_start_pos) == -1;
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
            /* To search backwards using regex we have to go back by chunks
             * and search forwards. This can mean that a match is found before
             * the limit, so in this case we unset the false match */
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

static Status rs_find_prev_str(const char *str, size_t str_len, size_t point,
                               size_t limit, size_t *match_point,
                               int *found_match, RegexSearch *search)
{
    size_t search_length, search_point;
    size_t mpoint = 0, mlength = 0;
    size_t start_point = point;
    Status status = STATUS_SUCCESS;
    int found = 0;

    /* Go back through the buffer by chunks and search forwards to
     * accomplish a reverse search */

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
                /* Continue searching as there could be matches later in
                 * the buffer which must be matched first when searching
                 * backwards */
            } else {
                break;
            }
        } while (STATUS_IS_SUCCESS(status) && search_point < start_point);

        if (*found_match || !STATUS_IS_SUCCESS(status)) {
            if (*found_match &&
                (mpoint != *match_point || search->return_code < 1)) {
                /* Because we have to search to the end of a chunk even
                 * when a match has been found, match data is lost. Here
                 * we detect that match data is lost or invalid and 
                 * perform a mini search to populate return_code,
                 * output_vector, etc... again with the correct match data */
                /* TODO Store match data in another way temporarily so that 
                 * it can simply be used at this point without the need
                 * for another search */
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
    search->return_code = pcre_exec(search->regex, search->study, str, limit,
                                    point, 0, search->output_vector,
                                    OUTPUT_VECTOR_SIZE);

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

