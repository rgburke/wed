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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "wed_syntax.h"
#include "regex_util.h"
#include "util.h"
#include "build_config.h"
#include "config.h"

static Status ws_load(SyntaxDefinition *, const char *syntax_type);
static SyntaxMatches *ws_generate_matches(const SyntaxDefinition *,
                                          const char *str, size_t str_len,
                                          size_t offset);
static void ws_add_match(SyntaxMatches *, const SyntaxMatch *);
static int ws_match_cmp(const void *, const void *);
static void ws_free(SyntaxDefinition *syn_def);

SyntaxDefinition *ws_new(Session *sess)
{
    WedSyntaxDefinition *wed_def = malloc(sizeof(WedSyntaxDefinition));
    RETURN_IF_NULL(wed_def);

    wed_def->sess = sess;
    wed_def->patterns = NULL;

    wed_def->syn_def.load = ws_load;
    wed_def->syn_def.generate_matches = ws_generate_matches;
    wed_def->syn_def.free = ws_free;

    return (SyntaxDefinition *)wed_def;
}

Status ws_new_pattern(SyntaxPattern **syn_pattern_ptr, const Regex *regex,
                      SyntaxToken token)
{
    assert(syn_pattern_ptr != NULL);
    assert(regex != NULL);
    assert(!is_null_or_empty(regex->regex_pattern));

    SyntaxPattern *syn_pattern = malloc(sizeof(SyntaxPattern));

    if (syn_pattern == NULL) {
        return OUT_OF_MEMORY("Unable to allocate SyntaxPattern");
    }

    memset(syn_pattern, 0, sizeof(SyntaxPattern));

    Status status = ru_compile_custom_error_msg(&syn_pattern->regex, regex, 
                                                "pattern ");

    if (!STATUS_IS_SUCCESS(status)) {
        free(syn_pattern);
        return status; 
    }

    syn_pattern->token = token;
    *syn_pattern_ptr = syn_pattern;

    return STATUS_SUCCESS;
}

void ws_free_pattern(SyntaxPattern *syn_pattern)
{
    if (syn_pattern == NULL) {
        return;
    }

    ru_free_instance(&syn_pattern->regex);
    free(syn_pattern);
}

static Status ws_load(SyntaxDefinition *syn_def, const char *syntax_type)
{
    WedSyntaxDefinition *wed_def = (WedSyntaxDefinition *)syn_def;
    Session *sess = wed_def->sess;

    /* Config definition loading in wed is done in a generic way. For this
     * reason there is currently no functionality allowing a loaded config
     * definition to be returned to the calling function. Therefore a rather
     * roundabout method is used where the config code puts the definition into
     * the syntax managers hashmap itself. We then check the map to see if
     * the definition was successfully loaded after the call to
     * cf_load_config_def below. If so we get the definition, copy the patterns
     * reference it has and free the definition so that it is not overwritten
     * by this current instance in sm_load_definition (which would cause a
     * memory leak if it happened) */

    cf_load_config_def(sess, CT_SYNTAX, syntax_type);

    SyntaxManager *sm = &sess->sm;
    WedSyntaxDefinition *loaded_def = hashmap_get(sm->syn_defs, syntax_type);

    if (loaded_def != NULL) {
        wed_def->patterns = loaded_def->patterns;
        free(loaded_def);
    } else {
        return st_get_error(ERR_INVALID_SYNTAXTYPE,
                            "No syntax type \"%s\" exists", syntax_type);
    }

    return STATUS_SUCCESS;
}

/* Run SyntaxDefintion against buffer substring to determine
 * tokens present and return these matches */
static SyntaxMatches *ws_generate_matches(const SyntaxDefinition *syn_def,
                                          const char *str, size_t str_len,
                                          /* Offset into buffer str was taken
                                           * from */
                                          size_t offset)
{
    assert(str != NULL);

    SyntaxMatches *syn_matches = sy_new_matches(offset);

    if (syn_matches == NULL || str_len == 0) {
        return syn_matches;
    }

    const WedSyntaxDefinition *wed_def = (WedSyntaxDefinition *)syn_def;
    const SyntaxPattern *pattern = wed_def->patterns;
    SyntaxMatch syn_match;
    RegexResult result;
    Status status;

    /* Run each SyntaxPattern against str */
    while (pattern != NULL) {
        size_t offset = 0;

        /* Find all matches in str ensuring we don't 
         * exceed MAX_SYNTAX_MATCH_NUM */
        while (syn_matches->match_num < MAX_SYNTAX_MATCH_NUM &&
               offset < str_len) {
            status = ru_exec(&result, &pattern->regex, str, str_len, offset);

            if (!(STATUS_IS_SUCCESS(status) && result.match)) {
                st_free_status(status);
                /* Failure or no matches in the remainder of str
                 * so we're finished with this SyntaxPatten */
                break;
            }

            syn_match.offset = result.output_vector[0];
            syn_match.length = result.match_length;
            syn_match.token = pattern->token;

            ws_add_match(syn_matches, &syn_match);

            offset = result.output_vector[0] + result.match_length;
        } 

        pattern = pattern->next;
    }

    /* Order matches by offset then length */
    qsort(syn_matches->matches, syn_matches->match_num,
          sizeof(SyntaxMatch), ws_match_cmp);

    return syn_matches;
}

static void ws_add_match(SyntaxMatches *syn_matches,
                         const SyntaxMatch *syn_match)
{
    if (syn_matches->match_num == 0) {
        syn_matches->matches[syn_matches->match_num++] = *syn_match;
        return;
    }

    /* Large matches take precedence over smaller matches. Below we
     * check if the range of this match is already covered by an
     * existing larger match e.g. if a string contains a keyword
     * like int this ensures the whole range matched by the string
     * is considered as a string and the int part is not highlighted
     * differently */
    /* TODO Of course in future for more advanced syntax highlighting
     * it is useful to allow tokens to contain certain child tokens
     * and the method we use below will have to be updated.
     * e.g. C string format specifiers highlighted differently to
     * the rest of the string */
    for (size_t k = 0; k < syn_matches->match_num; k++) {
        if (syn_match->offset >= syn_matches->matches[k].offset &&
            syn_match->offset < syn_matches->matches[k].offset + 
                                syn_matches->matches[k].length) {
            return; 
        }
    }

    syn_matches->matches[syn_matches->match_num++] = *syn_match;
}

static int ws_match_cmp(const void *v1, const void *v2)
{
    const SyntaxMatch *m1 = (const SyntaxMatch *)v1;
    const SyntaxMatch *m2 = (const SyntaxMatch *)v2;

    if (m1->offset == m2->offset) {
        return (int)m2->length - (int)m1->length;
    }

    return (int)m1->offset - (int)m2->offset;
}

static void ws_free(SyntaxDefinition *syn_def)
{
    if (syn_def == NULL) {
        return;
    }

    WedSyntaxDefinition *wed_def = (WedSyntaxDefinition *)syn_def;
    
    SyntaxPattern *next;
    SyntaxPattern *current = wed_def->patterns;

    while (current != NULL) {
        next = current->next;
        ws_free_pattern(current);
        current = next;
    }

    free(wed_def);
}

