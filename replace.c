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

#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "replace.h"
#include "search.h"
#include "status.h"
#include "util.h"

static Status rp_ts_replace(Buffer *, const char *rep_text, size_t rep_length);
static Status rp_rs_replace(Buffer *, const char *rep_text, size_t rep_length);
static Status rp_rs_get_new_replace_str_length(const RegexSearch *,
                                               size_t *new_length,
                                               size_t rep_length);
static void rp_rs_replace_backreferences(Buffer *, const RegexSearch *,
                                         char *new_rep_text,
                                         size_t new_rep_length, const char *,
                                         size_t rep_length);

Status rp_replace_init(BufferSearch *search, const char *rep_text,
                       size_t rep_length)
{
    if (search->search_type == BST_TEXT) {
        return STATUS_SUCCESS;
    }

    RegexSearch *regex_search = &search->type.regex;
    RegexReplace *regex_replace = &regex_search->regex_replace;
    memset(regex_replace, 0, sizeof(RegexReplace));

    size_t back_ref_num;
    size_t *back_ref;
    size_t i;

    /* Scan replace text for backreferences. Store data about each
     * backreference found to make it easier to generate the 
     * final replace text after a search match has been found */
    for (size_t k = 0; k < rep_length; k++) {
        if (rep_text[k] == '\\' && 
            (k == 0 || rep_text[k - 1] != '\\') &&
            k + 1 < rep_length) {

            i = k + 1;
            back_ref_num = 0;

            /* Convert backreference string digits into number */
            for (; i < rep_length && isdigit(rep_text[i]); i++) {
                back_ref_num = (back_ref_num * 10) + (rep_text[i] - 48);
            }

            if (i - k > 1) {
                if (back_ref_num > MAX_CAPTURE_GROUP_NUM) {
                    return st_get_error(ERR_TOO_MANY_REGEX_CAPTURE_GROUPS,
                                        "Backreference \\%zu in replace text "
                                        "exceeds maximum capture group number "
                                        "\\%zu that wed can capture",
                                        back_ref_num, MAX_CAPTURE_GROUP_NUM);                    
                }

                if (regex_replace->back_ref_occurrences >=
                    MAX_BACK_REF_OCCURRENCES) {
                    return st_get_error(ERR_TOO_MANY_REGEX_BACKREFERENCES,
                                        "Number of backreferences in replace "
                                        "text exceeds maximum number of "
                                        "backreferences %zu that can occur",
                                        MAX_BACK_REF_OCCURRENCES);
                }

                back_ref = regex_replace->back_refs[
                               regex_replace->back_ref_occurrences++
                           ];

                /* See regex_search.h for explanation */
                back_ref[0] = back_ref_num;
                back_ref[1] = k;
                back_ref[2] = i - k;
            }
        }
    }

    return STATUS_SUCCESS;
}

Status rp_replace_current_match(Buffer *buffer, const char *rep_text,
                                size_t rep_length)
{
    BufferSearch *search = &buffer->search;

    assert(search->last_match_pos.line_no > 0);
    assert(rep_text != NULL);

    /* Check a match has actually been found i.e.
     * line_no should be greater than zero */
    if (search->last_match_pos.line_no == 0) {
        return STATUS_SUCCESS;
    } 

    Status status;

    if (search->search_type == BST_TEXT) {
        status = rp_ts_replace(buffer, rep_text, rep_length); 
    } else if (search->search_type == BST_REGEX) {
        status = rp_rs_replace(buffer, rep_text, rep_length); 
    } 

    return status;
}

static Status rp_ts_replace(Buffer *buffer, const char *rep_text,
                            size_t rep_length)
{
    return bf_replace_string(buffer, buffer->search.opt.pattern_len,
                             rep_text, rep_length, 1);
}

static Status rp_rs_replace(Buffer *buffer, const char *rep_text,
                            size_t rep_length)
{
    BufferSearch *search = &buffer->search;
    RegexSearch *regex_search = &search->type.regex;
    RegexReplace *regex_replace = &regex_search->regex_replace;

    assert(regex_search->return_code > 0);
    
    if (regex_replace->back_ref_occurrences == 0) {
        return bf_replace_string(buffer, regex_search->match_length,
                                 rep_text, rep_length, 1);
    }

    size_t new_rep_length;
    RETURN_IF_FAIL(rp_rs_get_new_replace_str_length(regex_search,
                                                    &new_rep_length,
                                                    rep_length));

    char *new_rep_text = malloc(new_rep_length + 1);

    if (new_rep_text == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to allocate memory to replace");
    }

    rp_rs_replace_backreferences(buffer, regex_search, new_rep_text,
                                 new_rep_length, rep_text, rep_length);

    Status status = bf_replace_string(buffer, regex_search->match_length,
                                      new_rep_text, new_rep_length, 1);

    free(new_rep_text);

    return status;
}

/* Calculate length of replace text after replacing backreferences
 * with captured group text */
static Status rp_rs_get_new_replace_str_length(const RegexSearch *regex_search,
                                               size_t *new_length,
                                               size_t rep_length)
{
    const RegexReplace *regex_replace = &regex_search->regex_replace;
    const size_t *back_ref;
    *new_length = rep_length;

    for (size_t k = 0; k < regex_replace->back_ref_occurrences; k++) {
        back_ref = regex_replace->back_refs[k];

        if (back_ref[0] >= (size_t)regex_search->return_code) {
            return st_get_error(ERR_INVALID_CAPTURE_GROUP_BACKREFERENCE,
                                "Backreference \\%zu in replace text is "
                                "greater than the number of groups captured %d",
                                back_ref[0], regex_search->return_code - 1);
        }

        /* Add on length of captured group */
        *new_length += regex_search->output_vector[(back_ref[0] * 2) + 1]
                       - regex_search->output_vector[back_ref[0] * 2];
        /* Subtract the length of the backreference in string format */
        *new_length -= back_ref[2];
    }

    return STATUS_SUCCESS;
}

static void rp_rs_replace_backreferences(Buffer *buffer,
                                         const RegexSearch *regex_search,
                                         char *new_rep_text,
                                         size_t new_rep_length,
                                         const char *rep_text,
                                         size_t rep_length)
{
    const RegexReplace *regex_replace = &regex_search->regex_replace;
    const size_t *back_ref;
    size_t str_len;
    /* Keep track of our positions in {new_}rep_text */
    size_t rep_index = 0;
    size_t new_rep_index = 0;

    for (size_t k = 0; k < regex_replace->back_ref_occurrences; k++) {
        back_ref = regex_replace->back_refs[k]; 
        str_len = back_ref[1] - rep_index;

        /* Copy any text since the last backreference */
        if (str_len > 0) {
            memcpy(new_rep_text + new_rep_index,
                   rep_text + rep_index, str_len);
            rep_index += str_len;
            new_rep_index += str_len;
        }

        /* Captured group text length */
        str_len = regex_search->output_vector[(back_ref[0] * 2) + 1] -
                  regex_search->output_vector[back_ref[0] * 2];

        if (str_len > 0) {
            /* Copy captured group text into new_rep_text */
            gb_get_range(buffer->data,
                         regex_search->output_vector[back_ref[0] * 2],
                         new_rep_text + new_rep_index, str_len);
            new_rep_index += str_len;
            rep_index += back_ref[2];
        }
    }

    /* Copy any remaining text after the last backreference */
    if (new_rep_index < new_rep_length) {
        memcpy(new_rep_text + new_rep_index, rep_text + rep_index,
               rep_length - rep_index);
    }

    *(new_rep_text + new_rep_length) = '\0';
}
