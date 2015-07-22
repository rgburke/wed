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

#ifndef WED_SYNTAX_H
#define WED_SYNTAX_H

#include "regex_util.h"
#include "shared.h"
#include "status.h"

typedef enum {
    ST_KEYWORD
} SyntaxToken;

typedef struct SyntaxPattern SyntaxPattern;

struct SyntaxPattern {
    RegexInstance regex;
    SyntaxToken token;
    SyntaxPattern *next;
};

typedef struct {
    char *name;
    SyntaxPattern *patterns;
} SyntaxDefinition;

int sy_str_to_token(SyntaxToken *, const char *);
Status sy_new_pattern(SyntaxPattern **, const Regex *, SyntaxToken);
void syn_free_pattern(SyntaxPattern *);
SyntaxDefinition *sy_new_def(const char *, SyntaxPattern *);
void sy_free_def(SyntaxDefinition *);

#endif
