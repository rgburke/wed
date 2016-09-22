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

/* Define custom YYLTYPE for locations which includes filename */
typedef struct {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    const char *file_name;
} YYLTYPE, ParseLocation;

/* Inform Bison that we have a custom YYLTYPE */
#define YYLTYPE_IS_DECLARED 1

/* Update location data every type Bison reduces a rule */
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

/* When parsing a config file we build an AST.
 * Below are the nodes types */
typedef enum {
    NT_VALUE,
    NT_VALUE_LIST,
    NT_IDENTIFIER,
    NT_ASSIGNMENT,
    NT_FUNCTION_CALL,
    NT_STATEMENT,
    NT_STATEMENT_BLOCK
} ASTNodeType;

/* This struct acts as a base class. The *Node 
 * structures below have an ASTNode as their first member
 * variable, which allows them all to be cast to ASTNode's.
 * This struct contains properties common to all nodes namely
 * type and location */
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
    List *values;
} ValueListNode;

typedef struct {
    ASTNode type;
    char *name; /* Variable name */
} IdentifierNode;

/* Used for variable assignment where
 * left is a IdentifierNode and right is
 * a ValueNode */
typedef struct {
    ASTNode type;
    ASTNode *left;
    ASTNode *right;
} ExpressionNode;

typedef struct StatementNode StatementNode;

/* A linked list is used to store multiple statements. */
struct StatementNode {
    ASTNode type;
    ASTNode *node;
    StatementNode *next;
};

/* Used to represent wed block definitions
 * e.g. syntax { ... }
 * i.e. Used to store statements grouped in a named block */
typedef struct {
    ASTNode type;
    char *block_name;
    ASTNode *node;
} StatementBlockNode;

/* A statement can be a named block which can in turn
 * contain statements */

ValueNode *cp_new_valuenode(const ParseLocation *, Value);
ValueListNode *cp_new_valuelistnode(const ParseLocation *, ASTNode *val);
IdentifierNode *cp_new_identifiernode(const ParseLocation *, const char *name);
ExpressionNode *cp_new_expressionnode(const ParseLocation *, ASTNodeType,
                                      ASTNode *left, ASTNode *right);
StatementNode *cp_new_statementnode(const ParseLocation *, ASTNode *);
StatementBlockNode *cp_new_statementblocknode(const ParseLocation *,
                                              const char *block_name,
                                              ASTNode *);
int cp_convert_to_bool_value(const char *svalue, Value *);
int cp_convert_to_int_value(const char *svalue, Value *);
int cp_convert_to_string_value(const char *svalue, Value *);
int cp_convert_to_regex_value(const char *rvalue, Value *);
int cp_convert_to_shell_command_value(const char *cmd_value, Value *);
int cp_add_statement_to_list(ASTNode *statememt_list, ASTNode *statememt);
int cp_add_value_to_list(ASTNode *val_list, ASTNode *val);
int cp_eval_ast(Session *, ConfigLevel, ASTNode *);
void cp_free_ast(ASTNode *);
void cp_update_parser_location(const char *yytext, const char *file_name);
Status cp_convert_to_config_error(Status, const ParseLocation *);
Status cp_get_config_error(ErrorCode, const ParseLocation *,
                           const char *format, ...);
void yyerror(Session *, ConfigLevel, const char *file_name, char const *error);
Status cp_parse_config_file(Session *, ConfigLevel,
                            const char *config_file_path);
Status cp_parse_config_string(Session *, ConfigLevel, const char *str);
void cp_start_scan_file(List *, FILE *);
void cp_start_scan_string(List *buffer_stack, const char *str);
void cp_finish_scan(List *buffer_stack);
ConfigLevel cp_determine_config_level(const char *var_name, ConfigLevel);

#endif
