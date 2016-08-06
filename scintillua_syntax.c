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
#include <assert.h>
#include "scintillua_syntax.h"
#include "util.h"

static Status sl_load(SyntaxDefinition *, const char *syntax_type);
static SyntaxMatches *sl_generate_matches(const SyntaxDefinition *,
                                          const char *str, size_t str_len,
                                          size_t offset);
static void sl_free(SyntaxDefinition *);

SyntaxDefinition *sl_new(Session *sess)
{
   ScintilluaSyntaxDefinition *sl_def =
       malloc(sizeof(ScintilluaSyntaxDefinition));

    RETURN_IF_NULL(sl_def);

    memset(sl_def, 0, sizeof(ScintilluaSyntaxDefinition));

    sl_def->ls = sess->ls;

    sl_def->syn_def.load = sl_load;
    sl_def->syn_def.generate_matches = sl_generate_matches;
    sl_def->syn_def.free = sl_free;

    return (SyntaxDefinition *)sl_def;
}

static Status sl_load(SyntaxDefinition *syn_def, const char *syntax_type)
{
    assert(!is_null_or_empty(syntax_type));
    ScintilluaSyntaxDefinition *sl_def = (ScintilluaSyntaxDefinition *)syn_def;

    RETURN_IF_FAIL(ls_load_syntax_def(sl_def->ls, syntax_type));

    sl_def->syntax_type = strdup(syntax_type);

    if (sl_def->syntax_type == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to copy syntax type");
    }
    
    return STATUS_SUCCESS;
}

static SyntaxMatches *sl_generate_matches(const SyntaxDefinition *syn_def,
                                          const char *str, size_t str_len,
                                          size_t offset)
{
    ScintilluaSyntaxDefinition *sl_def = (ScintilluaSyntaxDefinition *)syn_def;

    SyntaxMatches *syn_matches = ls_generate_matches(sl_def->ls,
                                                     sl_def->syntax_type,
                                                     str, str_len);

    if (syn_matches != NULL) {
        syn_matches->offset = offset;
    }

    return syn_matches;
}

static void sl_free(SyntaxDefinition *syn_def)
{
    if (syn_def == NULL) {
        return;
    }

    ScintilluaSyntaxDefinition *sl_def = (ScintilluaSyntaxDefinition *)syn_def;

    free(sl_def->syntax_type);
    free(sl_def);
}

