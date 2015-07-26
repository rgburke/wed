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
#include "syntax.h"
#include "regex_util.h"
#include "util.h"

static int sy_match_cmp(const void *, const void *);

int sy_str_to_token(SyntaxToken *token, const char *token_str)
{
    static const char *syn_tokens[] = {
        [ST_KEYWORD] = "keyword"
    };

    static const size_t token_num = sizeof(syn_tokens) / sizeof(const char *);

    for (size_t k = 0; k < token_num; k++) {
        if (strcmp(syn_tokens[k], token_str) == 0) {
            *token = k;
            return 1;
        }
    }

    return 0;
}

Status sy_new_pattern(SyntaxPattern **syn_pattern_ptr, const Regex *regex, SyntaxToken token)
{
    assert(syn_pattern_ptr != NULL);
    assert(regex != NULL);
    assert(!is_null_or_empty(regex->regex_pattern));

    SyntaxPattern *syn_pattern = malloc(sizeof(SyntaxPattern));

    if (syn_pattern == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to allocate SyntaxPattern");
    }

    memset(syn_pattern, 0, sizeof(SyntaxPattern));

    Status status = re_compile_custom_error_msg(&syn_pattern->regex, regex, 
                                                "pattern ");

    if (!STATUS_IS_SUCCESS(status)) {
        free(syn_pattern);
        return status; 
    }

    syn_pattern->token = token;
    *syn_pattern_ptr = syn_pattern;

    return STATUS_SUCCESS;
}

void syn_free_pattern(SyntaxPattern *syn_pattern)
{
    if (syn_pattern == NULL) {
        return;
    }

    re_free_instance(&syn_pattern->regex);
    free(syn_pattern);
}

SyntaxDefinition *sy_new_def(const char *name, SyntaxPattern *patterns)
{
    assert(!is_null_or_empty(name));
    assert(patterns != NULL);

    SyntaxDefinition *syn_def = malloc(sizeof(SyntaxDefinition));

    if (syn_def == NULL) {
        return NULL;
    }

    memset(syn_def, 0, sizeof(SyntaxDefinition));

    syn_def->name = strdupe(name);

    if (syn_def->name == NULL) {
        free(syn_def);
        return NULL;
    }

    syn_def->patterns = patterns;

    return syn_def;
}

void sy_free_def(SyntaxDefinition *syn_def)
{
    if (syn_def == NULL) {
        return;
    }

    free(syn_def->name);

    SyntaxPattern *next;

    while (syn_def->patterns != NULL) {
        next = syn_def->patterns->next;
        syn_free_pattern(syn_def->patterns);
        syn_def->patterns = next;
    }

    free(syn_def);
}

SyntaxMatches *sy_get_syntax_matches(const SyntaxDefinition *syn_def, 
                                     const char *str, size_t str_len,
                                     size_t offset)
{
    if (str_len == 0) {
        return NULL;
    }

    SyntaxMatches *syn_matches = malloc(sizeof(SyntaxMatches));

    if (syn_matches == NULL) {
        return NULL;
    }

    syn_matches->match_num = 0;
    syn_matches->current_match = 0;
    syn_matches->offset = offset;
    
    SyntaxPattern *pattern = syn_def->patterns;
    RegexResult result;
    Status status;

    while (pattern != NULL) {
        size_t offset = 0;

        do {
            status = re_exec(&result, &pattern->regex, str, str_len, offset);

            if (!(STATUS_IS_SUCCESS(status) && result.match)) {
                break; 
            }

            syn_matches->matches[syn_matches->match_num++] = (SyntaxMatch) {
                .offset = result.output_vector[0],
                .length = result.match_length,
                .token = pattern->token
            }; 

            offset = result.output_vector[0] + result.match_length;
        } while (syn_matches->match_num < MAX_SYNTAX_MATCH_NUM &&
                 offset < str_len);

        pattern = pattern->next;
    }

    qsort(syn_matches->matches, syn_matches->match_num, sizeof(SyntaxMatch), sy_match_cmp);

    return syn_matches;
}

static int sy_match_cmp(const void *v1, const void *v2)
{
    const SyntaxMatch *m1 = (const SyntaxMatch *)v1;
    const SyntaxMatch *m2 = (const SyntaxMatch *)v2;

    if (m1->offset == m2->offset) {
        return (int)m2->length - (int)m1->length;
    }

    return (int)m1->offset - (int)m2->offset;
}

const SyntaxMatch *sy_get_syntax_match(SyntaxMatches *syn_matches, size_t offset)
{
    if (syn_matches == NULL || syn_matches->match_num == 0 ||
        syn_matches->offset > offset) {
        return NULL;
    }

    offset -= syn_matches->offset;

    while (syn_matches->current_match < syn_matches->match_num) {
        const SyntaxMatch *syn_match = &syn_matches->matches[syn_matches->current_match];

        if (offset < syn_match->offset) {
            break;
        } else if (offset < syn_match->offset + syn_match->length) {
            return syn_match;
        }

        syn_matches->current_match++;
    }

    return NULL;
}
