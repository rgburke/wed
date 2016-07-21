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
#include "syntax_manager.h"
#include "wed_syntax.h"
#include "source_highlight_syntax.h"
#include "session.h"
#include "util.h"

typedef SyntaxDefinition *(*SyntaxDefinitionCreator)(Session *sess);

/* Map each syntax definition type to a syntax definition creator function.
 * This allows us to create instances of different syntax definition types in
 * a generic way */
static const SyntaxDefinitionCreator sm_syntax_def_types[] = {
    [SDT_WED] = ws_new
#if WED_SOURCE_HIGHLIGHT
    ,[SDT_SOURCE_HIGHLIGHT] = sh_new
#endif
};

static void sm_free_syn_def(SyntaxDefinition *);

int sm_init(SyntaxManager *sm)
{
    memset(sm, 0, sizeof(SyntaxManager));
    sm->syn_defs = new_hashmap();

    if (sm->syn_defs == NULL) {
        return 0;
    }

    return 1;
}

void sm_free(SyntaxManager *sm)
{
    free_hashmap_values(sm->syn_defs, (void (*)(void *))sm_free_syn_def);
    free_hashmap(sm->syn_defs);
}

static void sm_free_syn_def(SyntaxDefinition *syn_def)
{
    if (syn_def == NULL) {
        return;
    }

    syn_def->free(syn_def);
}

Status sm_load_definition(SyntaxManager *sm, Session *sess,
                          SyntaxDefinitionType type,
                          const char *syntax_type)
{
    assert(!is_null_or_empty(syntax_type));
    
    if (sm_has_def(sm, syntax_type)) {
        return STATUS_SUCCESS;
    }

    SyntaxDefinitionCreator creator = sm_syntax_def_types[type];
    SyntaxDefinition *syn_def = creator(sess);

    if (syn_def == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to create syntax definition");
    }

    Status status = syn_def->load(syn_def, syntax_type);

    if (!STATUS_IS_SUCCESS(status)) {
        sm_free_syn_def(syn_def);
        return status;
    }

    if (!hashmap_set(sm->syn_defs, syntax_type, syn_def)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to save syntax definition");
    }

    return STATUS_SUCCESS;
}

int sm_get_syntax_definition_type(const char *syn_def_type,
                                  SyntaxDefinitionType *type_ptr)
{
    static const char *syn_def_types[] = {
        [SDT_WED] = "wed"
#if WED_SOURCE_HIGHLIGHT
        ,[SDT_SOURCE_HIGHLIGHT] = "sh"
#endif
    };
    static const size_t syn_def_num = ARRAY_SIZE(syn_def_types, const char *);

    if (is_null_or_empty(syn_def_type)) {
        return 0;
    }

    for (size_t k = 0; k < syn_def_num; k++) {
        if (strcmp(syn_def_type, syn_def_types[k]) == 0) {
            if (type_ptr != NULL) {
                *type_ptr = k;
            }

            return 1;
        }
    }

    return 0;
}

const SyntaxDefinition *sm_get_def(const SyntaxManager *sm,
                                   const char *syntax_type)
{
    return hashmap_get(sm->syn_defs, syntax_type);
}

int sm_has_def(const SyntaxManager *sm, const char *syntax_type)
{
    return sm_get_def(sm, syntax_type) != NULL;
}
