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

#ifndef WED_PARSE_H
#define WED_PARSE_H

#include <stdarg.h>
#include "value.h"
#include "session.h"
#include "config.h"

typedef struct {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    const char *file_name;
} YYLTYPE, ParseLocation;

#define YYLTYPE_IS_DECLARED 1

#define YYLLOC_DEFAULT(Current, Rhs, N)                              \
    do {                                                             \
        if (N) {                                                     \
            (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;   \
            (Current).first_column = YYRHSLOC (Rhs, 1).first_column; \
            (Current).last_line    = YYRHSLOC (Rhs, N).last_line;    \
            (Current).last_column  = YYRHSLOC (Rhs, N).last_column;  \
            (Current).file_name    = YYRHSLOC (Rhs, 1).file_name;    \
        } else {                                                     \
            (Current).first_line   = (Current).last_line   =         \
              YYRHSLOC (Rhs, 0).last_line;                           \
            (Current).first_column = (Current).last_column =         \
              YYRHSLOC (Rhs, 0).last_column;                         \
            (Current).file_name    =  NULL;                          \
        }                                                            \
    } while (0)

typedef enum {
    NT_VALUE,
    NT_VARIABLE,
    NT_ASSIGNMENT,
    NT_REFERENCE,
    NT_STATEMENT,
    NT_STATEMENT_BLOCK
} ASTNodeType;

typedef struct {
    ASTNodeType node_type; 
    ParseLocation location;
} ASTNode;

typedef struct {
    ASTNode type;
    Value value;
} ValueNode;

typedef struct {
    ASTNode type;
    char *name;
} VariableNode;

typedef struct {
    ASTNode type;
    ASTNode *left;
    ASTNode *right;
} ExpressionNode;

typedef struct StatementNode StatementNode;

struct StatementNode {
    ASTNode type;
    ASTNode *node;
    StatementNode *next;
};

typedef struct {
    ASTNode type;
    char *block_name;
    ASTNode *node;
} StatementBlockNode;

ValueNode *cp_new_valuenode(const ParseLocation *, Value);
VariableNode *cp_new_variablenode(const ParseLocation *, const char *);
ExpressionNode *cp_new_expressionnode(const ParseLocation *, ASTNodeType, ASTNode *, ASTNode *);
StatementNode *cp_new_statementnode(const ParseLocation *, ASTNode *);
StatementBlockNode *cp_new_statementblocknode(const ParseLocation *, const char *, ASTNode *);
int cp_convert_to_bool_value(const char *, Value *);
int cp_convert_to_int_value(const char *, Value *);
int cp_convert_to_string_value(const char *, Value *);
int cp_convert_to_regex_value(const char *, Value *);
int cp_add_statement_to_list(ASTNode *, ASTNode *);
int cp_eval_ast(Session *, ConfigLevel, ASTNode *);
void cp_free_ast(ASTNode *);
void cp_update_parser_location(const char *, const char *);
Status cp_get_config_error(ErrorCode, const ParseLocation *, const char *, ...);
void yyerror(Session *, ConfigLevel, const char *, char const *);
Status cp_parse_config_file(Session *, ConfigLevel, const char *);
Status cp_parse_config_string(Session *, ConfigLevel, const char *);
void cp_start_scan_file(List *, FILE *);
void cp_start_scan_string(List *, const char *);
void cp_finish_scan(List *);

#endif
