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

#ifndef WED_SYNTAX_MANAGER_H
#define WED_SYNTAX_MANAGER_H

#include "syntax.h"
#include "hashmap.h"
#include "status.h"
#include "build_config.h"

/* The providers of syntax definitions available in wed */
typedef enum {
    SDT_WED /* Builtin wed definition - always available */
#if WED_SOURCE_HIGHLIGHT
    ,SDT_SOURCE_HIGHLIGHT /* GNU Source Highlight based definition, only
                             available if wed was compiled with support for
                             GNU Source Highlight */
#endif
} SyntaxDefinitionType;

/* Wrapper around hashmap used to store syntax definitions */
typedef struct {
    HashMap *syn_defs; /* Store syntax definitions by name */
} SyntaxManager;

struct Session;

int sm_init(SyntaxManager *);
void sm_free(SyntaxManager *);
Status sm_load_definition(SyntaxManager *, struct Session *,
                          SyntaxDefinitionType, const char *syntax_type);
int sm_get_syntax_definition_type(const char *syn_def_type,
                                  SyntaxDefinitionType *type_ptr);
const SyntaxDefinition *sm_get_def(const SyntaxManager *,
                                   const char *syntax_type);
int sm_has_def(const SyntaxManager *, const char *syntax_type);

#endif

