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

%{
#include <stdlib.h>
#include <stdio.h>
#include "session.h"
#include "config.h"
#include "config_parse_util.h"

int yylex(Session *, const char *file_path);
%}

/* Includes placed in config_parse.h */
%code requires { 
#include "session.h"
#include "config.h"
#include "config_parse_util.h"
}

/* yyparse arguments */
%parse-param { Session *sess }
%parse-param { ConfigLevel config_level }
%parse-param { const char *file_path }
/* yylex arguments */
%lex-param { Session *sess }
%lex-param { const char *file_path }
/* Bison provides more verbose and specific error messages */
%error-verbose
/* Track locations */
%locations

/* Semantic value data types */
%union {
    ASTNode *node;
    char *string;
}

/* Free discarded tokens */
%destructor { free($$); } <string>
%destructor { cp_free_ast($$); } <node>

/* Terminal symbols */
%token<string> TKN_INTEGER "integer"
%token<string> TKN_STRING "string"
%token<string> TKN_BOOLEAN "boolean"
%token<string> TKN_REGEX "regex"
%token<string> TKN_NAME "identifier"
%token TKN_ASSIGN "="
%token TKN_SEMI_COLON ";"
%token TKN_LEFT_BRACKET "{"
%token TKN_RIGHT_BRACKET "}"

/* All nonterminal symbols have type ASTNode * */
%type<node> value variable expression statememt statememt_list statement_block

/* Explicitly state start symbol */
%start program

%%

program:
       | statememt_list {
            cp_eval_ast(sess, config_level, $1); cp_free_ast($1);
         }
       ;

statememt_list: statememt { $$ = $1; }
              | statememt_list statememt {
                    if ($1 == NULL) {
                        $$ = $2;
                    } else {
                        cp_add_statement_to_list($1, $2);
                        $$ = $1;
                    }
                }
              ;

statememt: expression TKN_SEMI_COLON {
               $$ = (ASTNode *)cp_new_statementnode(&@$, $1);
           }
         | statement_block { $$ = (ASTNode *)cp_new_statementnode(&@$, $1); }
         | error TKN_SEMI_COLON { $$ = NULL; }
         ;

statement_block: TKN_NAME TKN_LEFT_BRACKET statememt_list TKN_RIGHT_BRACKET {
                     $$ = (ASTNode *)cp_new_statementblocknode(&@$, $1, $3);
                     free($1);
                 }
               | error TKN_RIGHT_BRACKET { $$ = NULL; } 
               ;

expression: variable TKN_ASSIGN value {
                $$ = (ASTNode *)cp_new_expressionnode(&@$, NT_ASSIGNMENT,
                                                      $1, $3);
            }
          | variable {
                $$ = (ASTNode *)cp_new_expressionnode(&@$, NT_REFERENCE,
                                                      $1, NULL);
            }
          ;

variable: TKN_NAME { $$ = (ASTNode *)cp_new_variablenode(&@1, $1); free($1); }
        ;

value: TKN_INTEGER {
           Value value; cp_convert_to_int_value($1, &value);
           $$ = (ASTNode *)cp_new_valuenode(&@1, value); free($1);
       }
     | TKN_STRING  {
           Value value; cp_convert_to_string_value($1, &value);
           $$ = (ASTNode *)cp_new_valuenode(&@1, value); free($1);
       }
     | TKN_BOOLEAN {
           Value value; cp_convert_to_bool_value($1, &value);
           $$ = (ASTNode *)cp_new_valuenode(&@1, value); free($1);
       }
     | TKN_REGEX   {
           Value value; cp_convert_to_regex_value($1, &value);
           $$ = (ASTNode *)cp_new_valuenode(&@1, value); free($1);
       }
     ;

%%

