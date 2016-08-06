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

#include "gnu_source_highlight_syntax.h"
#include "session.h"
#include "config.h"

/* The following three function declarations are the exposed C interface
 * from source_highlight.cc. As these functions only need to be called
 * from this file they are declared here rather than expose them publicly 
 * in a header file */
Status sh_init(SHSyntaxDefinition *, const char *lang_dir, const char *lang);
SyntaxMatches *sh_tokenize(const SHSyntaxDefinition *, const char *str,
                                  size_t str_len);
void sh_free_tokenizer(SHSyntaxDefinition *);

static Status sh_load(SyntaxDefinition *, const char *syntax_type);
static SyntaxMatches *sh_generate_matches(const SyntaxDefinition *,
                                          const char *str, size_t str_len,
                                          size_t offset);
static void sh_free(SyntaxDefinition *);

SyntaxDefinition *sh_new(Session *sess)
{
    SHSyntaxDefinition *sh_def = malloc(sizeof(SHSyntaxDefinition));

    RETURN_IF_NULL(sh_def);

    sh_def->tokenizer = NULL;
    sh_def->sess = sess;

    sh_def->syn_def.load = sh_load;
    sh_def->syn_def.generate_matches = sh_generate_matches;
    sh_def->syn_def.free = sh_free;

    return (SyntaxDefinition *)sh_def;
}

static Status sh_load(SyntaxDefinition *syn_def, const char *syntax_type)
{
    SHSyntaxDefinition *sh_def = (SHSyntaxDefinition *)syn_def;

    const char *shdd = cf_string(sh_def->sess->config, CV_SHDATADIR);

    return sh_init(sh_def, shdd, syntax_type);
}

static SyntaxMatches *sh_generate_matches(const SyntaxDefinition *syn_def,
                                          const char *str, size_t str_len,
                                          size_t offset)
{
    SHSyntaxDefinition *sh_def = (SHSyntaxDefinition *)syn_def;
    
    SyntaxMatches *syn_matches = sh_tokenize(sh_def, str, str_len);

    if (syn_matches != NULL) {
        syn_matches->offset = offset;
    }

    return syn_matches;
}

static void sh_free(SyntaxDefinition *syn_def)
{
    if (syn_def == NULL) {
        return;
    }

    SHSyntaxDefinition *sh_def = (SHSyntaxDefinition *)syn_def;

    sh_free_tokenizer(sh_def);
    free(sh_def);
}

