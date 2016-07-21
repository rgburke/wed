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

#ifndef WED_WED_SYNTAX_H
#define WED_WED_SYNTAX_H

#include "syntax.h"
#include "regex_util.h"
#include "session.h"

typedef struct SyntaxPattern SyntaxPattern;

/* Used to tokenize buffer content */
struct SyntaxPattern {
    RegexInstance regex; /* Pattern run against buffer content */
    SyntaxToken token; /* Token that matched buffer content corresponds with */
    SyntaxPattern *next; /* SynaxtPattern's are stored in a linked list */
};

/* Wed's own syntax definitions exposed by implementing the SyntaxDefinition
 * interface */
typedef struct {
    SyntaxDefinition syn_def; /* Interface */
    SyntaxPattern *patterns; /* Syntax patterns as defined in config */
    Session *sess; /* Reference to session, required when loading config
                      definitions */
} WedSyntaxDefinition;

SyntaxDefinition *ws_new(Session *);
Status ws_new_pattern(SyntaxPattern **syn_pattern_ptr, const Regex *,
                      SyntaxToken);
void ws_free_pattern(SyntaxPattern *);

#endif
