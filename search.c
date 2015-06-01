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

#include <string.h>
#include <assert.h>
#include "search.h"
#include "util.h"

Status bs_init(BufferSearch *search, const char *pattern, size_t pattern_len)
{
    assert(search != NULL);
    assert(pattern_len > 0);
    assert(!is_null_or_empty(pattern));

    search->opt.pattern = strdupe(pattern);

    if (search->opt.pattern == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - Unable to copy pattern");
    }

    search->opt.pattern_len = pattern_len;

    Status status;

    if (search->search_type == BST_TEXT) {
        status = ts_init(&search->type.text, &search->opt);
    } else if (search->search_type == BST_REGEX) {
        status = rs_init(&search->type.regex, &search->opt);
    }

    if (!STATUS_IS_SUCCESS(status)) {
        free(search->opt.pattern);
        search->opt.pattern = NULL;
    }

    search->last_search_type = search->search_type;

    return status;
}

Status bs_reinit(BufferSearch *search, const char *pattern, size_t pattern_len)
{
    bs_free(search);
    return bs_init(search, pattern, pattern_len);
}

Status bs_init_default_opt(BufferSearch *search)
{
    search->search_type = BST_TEXT;
    search->opt.forward = 1;
    search->opt.case_insensitive = 1;

    return STATUS_SUCCESS;
}

void bs_free(BufferSearch *search)
{
    if (search == NULL) {
        return;
    }

    free(search->opt.pattern);

    if (search->last_search_type == BST_TEXT) {
        ts_free(&search->type.text);
    } else if (search->last_search_type == BST_REGEX) {
        rs_free(&search->type.regex);
    }

    search->opt.pattern = NULL;
    search->opt.pattern_len = 0;
}

Status bs_find_next(BufferSearch *search, const BufferPos *start_pos, int *found_match)
{
    assert(search != NULL);
    assert(start_pos != NULL);
    assert(found_match != NULL);

    *found_match = 0;

    BufferPos pos = *start_pos;

    if (search->opt.forward && bp_compare(&pos, &search->last_match_pos) == 0) {
        bp_next_char(&pos);
    }

    size_t match_point;
    Status status;

    if (search->search_type == BST_TEXT) {
        if (search->opt.forward) {
            status = ts_find_next(&search->type.text, &search->opt, &pos, found_match, &match_point);
        } else {
            status = ts_find_prev(&search->type.text, &search->opt, &pos, found_match, &match_point);
        }
    } else if (search->search_type == BST_REGEX) {
        if (search->opt.forward) {
            status = rs_find_next(&search->type.regex, &search->opt, &pos, found_match, &match_point);
        } else {
            status = rs_find_prev(&search->type.regex, &search->opt, &pos, found_match, &match_point);
        }
    }

    RETURN_IF_FAIL(status);

    if (*found_match) {
        search->last_match_pos = bp_init_from_offset(match_point, &pos);
    }

    return STATUS_SUCCESS;
}
