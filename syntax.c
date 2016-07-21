/*
 * Copyright (C) 2016 Richard Burke
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
#include "syntax.h"

int sy_str_to_token(SyntaxToken *token, const char *token_str)
{
    static const char *syn_tokens[] = {
        [ST_NORMAL]     = "normal",
        [ST_COMMENT]    = "comment",
        [ST_CONSTANT]   = "constant",
        [ST_SPECIAL]    = "special",
        [ST_IDENTIFIER] = "identifier",
        [ST_STATEMENT]  = "statement",
        [ST_TYPE]       = "type",
        [ST_ERROR]      = "error",
        [ST_TODO]       = "todo"
    };

    for (size_t k = 0; k < ST_ENTRY_NUM; k++) {
        if (strcmp(syn_tokens[k], token_str) == 0) {
            *token = k;
            return 1;
        }
    }

    return 0;
}

SyntaxMatches *sy_new_matches(size_t offset)
{
    SyntaxMatches *syn_matches = malloc(sizeof(SyntaxMatches));
    RETURN_IF_NULL(syn_matches);

    memset(syn_matches, 0, sizeof(SyntaxMatches));
    syn_matches->offset = offset;

    return syn_matches;
}

int sy_add_match(SyntaxMatches *syn_matches, const SyntaxMatch *syn_match)
{
    if (syn_matches->match_num == MAX_SYNTAX_MATCH_NUM) {
        return 0;
    }

    syn_matches->matches[syn_matches->match_num++] = *syn_match;

    return 1;
}

/* Get the SyntaxMatch whose range contains the buffer offset.
 * If no such SyntaxMatch exists then return NULL.
 * This function is used to determine if this position in the
 * buffer requires custom colouring based on the SyntaxMatch token */
const SyntaxMatch *sy_get_syntax_match(SyntaxMatches *syn_matches,
                                       size_t offset)
{
    if (syn_matches == NULL || syn_matches->match_num == 0 ||
        syn_matches->offset > offset) {
        return NULL;
    }

    /* Convert buffer offset into buffer substring offset */
    offset -= syn_matches->offset;
    const SyntaxMatch *syn_match;

    /* syn_matches->current_match is the index of the last
     * SyntaxMatch we returned, so start checking from there */
    while (syn_matches->current_match < syn_matches->match_num) {
        syn_match = &syn_matches->matches[syn_matches->current_match];

        if (offset < syn_match->offset) {
            /* This offset isn't in a match yet */
            break;
        } else if (offset < syn_match->offset + syn_match->length) {
            return syn_match;
        }

        /* This offset exceeds the current SyntaxMatch's range so move
         * onto the next one */
        syn_matches->current_match++;
    }

    return NULL;
}

