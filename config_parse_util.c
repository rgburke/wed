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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "session.h"
#include "config.h"
#include "config_parse_util.h"
#include "config_parse.h"
#include "status.h"
#include "util.h"

void yyrestart(FILE *);
extern int yylineno;

ValueNode *new_valuenode(Value value)
{
    ValueNode *val_node = malloc(sizeof(ValueNode));
    RETURN_IF_NULL(val_node);

    val_node->type.node_type = NT_VALUE;
    val_node->value = value;

    return val_node;
}

VariableNode *new_variablenode(const char *var_name)
{
    VariableNode *var_node = malloc(sizeof(VariableNode));
    RETURN_IF_NULL(var_node);

    var_node->type.node_type = NT_VARIABLE;
    var_node->name = strdupe(var_name);

    return var_node;
}

ExpressionNode *new_expressionnode(ASTNodeType node_type, ASTNode *left, ASTNode *right)
{
    ExpressionNode *exp_node = malloc(sizeof(ExpressionNode));
    RETURN_IF_NULL(exp_node);

    exp_node->type.node_type = node_type;
    exp_node->left = left;
    exp_node->right = right;

    return exp_node;
}

StatementNode *new_statementnode(ASTNode *node)
{
    StatementNode *stm_node = malloc(sizeof(StatementNode));
    RETURN_IF_NULL(stm_node);

    stm_node->type.node_type = NT_STATEMENT;
    stm_node->node = node;
    stm_node->next = NULL;

    return stm_node;
}

int convert_to_bool_value(const char *svalue, Value *value)
{
    if (svalue == NULL || value == NULL) {
        return 0;
    }

    if (strncmp(svalue, "true", 5) == 0 || strncmp(svalue, "1", 2) == 0) {
        value->val.ival = 1;
    } else if (strncmp(svalue, "false", 6) == 0 || strncmp(svalue, "0", 2) == 0) {
        value->val.ival = 0;
    } else {
        return 0;
    }

    value->type = VAL_TYPE_BOOL;

    return 1;
}

int convert_to_int_value(const char *svalue, Value *value)
{
    if (svalue == NULL || value == NULL) {
        return 0;
    }

    char *end_ptr;
    errno = 0;

    long val = strtol(svalue, &end_ptr, 10);

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
        (errno != 0 && val == 0) || end_ptr == svalue) {
        return 0;
    }

    value->type = VAL_TYPE_INT;
    value->val.ival = val;

    return 1;
}

int convert_to_string_value(const char *svalue, Value *value)
{
    if (svalue == NULL || value == NULL) {
        return 0;
    }

    size_t length = strlen(svalue);
    char *processed;

    /* Empty string "" */
    if (length <= 2) {
        processed = malloc(sizeof(char));

        if (processed == NULL) {
            return 0;
        }

        *processed = '\0';
        return 1;
    } 

    const char *iter = svalue;
    size_t esc_count = 0;

    while (*iter++) {
        if (*iter == '\\') {
            esc_count++;

            if (*(iter + 1) == '\\') {
                iter++;
            }    
        }
    }

    processed = malloc(sizeof(char) * (length - esc_count) - 1);

    if (processed == NULL) {
        return 0;
    }

    iter = svalue;
    const char *end = svalue + length - 1;
    char *proc = processed;

    while (++iter != end) {
        if (*iter == '\\') {
            switch (*(iter + 1)) {
                case '\\':
                    {
                        *proc++ = '\\';
                        break;
                    }
                case '"':
                    {
                        *proc++ = '"';
                        break;
                    }
                case 'n':
                    {
                        *proc++ = '\n';
                        break;
                    }
                case 't':
                    {
                        *proc++ = '\t';
                        break;
                    }
                default:
                    {
                        break;
                    }
            }

            iter++;
        } else {
            *proc++ = *iter;
        }    
    }

    *proc = '\0';

    value->type = VAL_TYPE_STR;
    value->val.sval = processed;

    return 1;
}

int add_statement_to_list(ASTNode *statememt_list, ASTNode *statememt)
{
    if (statememt_list == NULL || statememt == NULL ||
        statememt_list->node_type != NT_STATEMENT ||
        statememt->node_type != NT_STATEMENT) {
        return 0;
    }

    StatementNode *list = (StatementNode *)statememt_list;
    StatementNode *stm_node = (StatementNode *)statememt;

    while (list->next != NULL) {
        list = list->next;
    }

    list->next = stm_node;

    return 1;
}

int eval_ast(Session *sess, ConfigLevel config_level, ASTNode *node)
{
    if (node == NULL) {
        return 0;
    }

    switch (node->node_type) {
        case NT_STATEMENT:
            {
                StatementNode *stm_node = (StatementNode *)node;

                while (stm_node != NULL) {
                    eval_ast(sess, config_level, stm_node->node);
                    stm_node = stm_node->next;
                }

                break;
            }
        case NT_ASSIGNMENT:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                VariableNode *var_node = (VariableNode *)exp_node->left;
                ValueNode *val_node = (ValueNode *)exp_node->right;

                if (var_node == NULL || val_node == NULL) {
                    return 0;
                }
                
                add_error(sess, set_var(sess, config_level, var_node->name, val_node->value));
                break;
            }
        default:
            return 0;
    }

    return 1;
}

void free_ast(ASTNode *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->node_type) {
        case NT_VALUE:
            {
                ValueNode *val_node = (ValueNode *)node;
                free_value(val_node->value);
                break;
            }
        case NT_VARIABLE:
            {
                VariableNode *var_node = (VariableNode *)node;
                free(var_node->name);
                break;
            }
        case NT_ASSIGNMENT:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                free_ast(exp_node->left);
                free_ast(exp_node->right);
                break;
            }
        case NT_STATEMENT:
            {
                StatementNode *stm_node = (StatementNode *)node;
                StatementNode *last;

                while (stm_node != NULL) {
                    free_ast(stm_node->node);
                    last = stm_node;
                    stm_node = stm_node->next;
                    free(last);
                }

                node = NULL;
                break;
            }
        default:
            break;
    }

    if (node != NULL) {
        free(node);
    }
}

void update_parser_location(int *yycolumn, int yylineno, int yyleng)
{
    yylloc.first_line = yylloc.last_line = yylineno;
    yylloc.first_column = *yycolumn;
    yylloc.last_column = *yycolumn + yyleng - 1;
    *yycolumn += yyleng;
}

Status get_config_error(ErrorCode error_code, const char *file_name, const char *format, ...)
{
    char new_format[MAX_ERROR_MSG_SIZE];

    snprintf(new_format, MAX_ERROR_MSG_SIZE, "%s:%d:%d: %s", file_name, 
             yylloc.first_line, yylloc.first_column, format);

    va_list arg_ptr;
    va_start(arg_ptr, format);
    Status status = get_custom_error(error_code, new_format, arg_ptr);
    va_end(arg_ptr);

    return status;
}

void yyerror(Session *sess, ConfigLevel config_level, const char *file_name, char const *error)
{
    (void)config_level;
    add_error(sess, get_config_error(ERR_INVALID_CONFIG_SYNTAX, file_name, error));
}

void reset_lexer(FILE *file)
{
    yyrestart(file);
    yylineno = 1;
}
