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

#define MAX_SYNTAX_MATCH_NUM 500

typedef enum {
    ST_NORMAL,
    ST_COMMENT,
    ST_CONSTANT,
    ST_SPECIAL,
    ST_IDENTIFIER,
    ST_STATEMENT,
    ST_TYPE,
    ST_ERROR,
    ST_TODO,
    ST_ENTRY_NUM
} SyntaxToken;

typedef struct SyntaxPattern SyntaxPattern;

struct SyntaxPattern {
    RegexInstance regex;
    SyntaxToken token;
    SyntaxPattern *next;
};

typedef struct {
    SyntaxPattern *patterns;
} SyntaxDefinition;

typedef struct {
    size_t offset;
    size_t length;
    SyntaxToken token;
} SyntaxMatch;

typedef struct {
    size_t match_num;
    size_t current_match;
    size_t offset;
    SyntaxMatch matches[MAX_SYNTAX_MATCH_NUM];
} SyntaxMatches;

int sy_str_to_token(SyntaxToken *, const char *);
Status sy_new_pattern(SyntaxPattern **, const Regex *, SyntaxToken);
void syn_free_pattern(SyntaxPattern *);
SyntaxDefinition *sy_new_def(SyntaxPattern *);
void sy_free_def(SyntaxDefinition *);
SyntaxMatches *sy_get_syntax_matches(const SyntaxDefinition *, const char *, size_t, size_t);
const SyntaxMatch *sy_get_syntax_match(SyntaxMatches *, size_t);

#endif
