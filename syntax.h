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

#ifndef WED_SYNTAX_H
#define WED_SYNTAX_H

#include <stddef.h>
#include "status.h"

#define MAX_SYNTAX_MATCH_NUM 500

/* The list of tokens available in wed. Syntax patterns can specify one 
 * of these tokens for matched buffer content, allowing wed to tokenize
 * buffer content. This data can be used by theme's to provide custom
 * colouring for each matched token */
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

/* Match data for SyntaxPattern's that have matched buffer content */
typedef struct {
    size_t offset; /* Offset into buffer substring (see SyntaxMatches) */
    size_t length; /* Length of match */
    SyntaxToken token; /* Token of the SyntaxPattern that matched */
} SyntaxMatch;

/* All token data for a SyntaxDefinition run on a buffer range */
struct SyntaxMatches {
    size_t match_num; /* Number of SyntaxMatch's found */
    size_t current_match; /* Used to keep track of the last SyntaxMatch
                             requested */
    size_t offset; /* SyntaxMatch's are generated from a buffer substring.
                      This is the offset into the buffer where the 
                      substring starts. This allows retrieving 
                      SyntaxMatch's by using the buffer's offset
                      rather than the substring offset */
    SyntaxMatch matches[MAX_SYNTAX_MATCH_NUM]; /* Array that stores matches */
};

typedef struct SyntaxMatches SyntaxMatches;

typedef struct SyntaxDefinition SyntaxDefinition;

/* Syntax definition interface. All syntax definition types that wed supports
 * implement this interface */
struct SyntaxDefinition {
    Status (*load)(SyntaxDefinition *, const char *syntax_type);
    SyntaxMatches *(*generate_matches)(const SyntaxDefinition *,
                                       const char *str, size_t str_len,
                                       size_t offset);
    void (*free)(SyntaxDefinition *);
};

int sy_str_to_token(SyntaxToken *, const char *token_str);
SyntaxMatches *sy_new_matches(size_t offset);
int sy_add_match(SyntaxMatches *, const SyntaxMatch *);
const SyntaxMatch *sy_get_syntax_match(SyntaxMatches *, size_t offset);

#endif
