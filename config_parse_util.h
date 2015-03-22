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

typedef enum {
    NT_VALUE,
    NT_VARIABLE,
    NT_ASSIGNMENT,
    NT_REFERENCE,
    NT_STATEMENT
} ASTNodeType;

typedef struct {
    ASTNodeType node_type; 
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

ValueNode *new_valuenode(Value);
VariableNode *new_variablenode(const char *);
ExpressionNode *new_expressionnode(ASTNodeType, ASTNode *, ASTNode *);
StatementNode *new_statementnode(ASTNode *);
int convert_to_bool_value(const char *, Value *);
int convert_to_int_value(const char *, Value *);
int convert_to_string_value(const char *, Value *);
int add_statement_to_list(ASTNode *, ASTNode *);
int eval_ast(Session *, ConfigLevel, ASTNode *);
void free_ast(ASTNode *);
void print_ast(ASTNode *);
void update_parser_location(int *, int, int);
Status get_config_error(ErrorCode, const char *, const char *, ...);
void yyerror(Session *, ConfigLevel, const char *, char const *);
Status parse_config_file(Session *, ConfigLevel, const char *);
Status parse_config_string(Session *, ConfigLevel, const char *);
void start_scan_string(const char *);
void finish_scan_string(void);

#endif
