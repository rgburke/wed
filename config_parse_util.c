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
#include <assert.h>
#include "session.h"
#include "config.h"
#include "config_parse_util.h"
#include "config_parse.h"
#include "status.h"
#include "util.h"
#include "file_type.h"

void yyrestart(FILE *);

typedef struct {
    char *var_name;
    ValueType value_type;
    void *value;
    const ParseLocation *location;
    int value_set;
} VariableAssignment;

static void cp_reset_parser_location(void);
static void cp_process_block(Session *, StatementBlockNode *);
static void cp_process_filetype_block(Session *, StatementBlockNode *);
static int cp_process_assignment(Session *, StatementNode *, VariableAssignment *, size_t);
static int cp_validate_block_vars(Session *, VariableAssignment *, size_t, const ParseLocation *, const char *, int);

ValueNode *cp_new_valuenode(const ParseLocation *location, Value value)
{
    ValueNode *val_node = malloc(sizeof(ValueNode));
    RETURN_IF_NULL(val_node);

    val_node->type.node_type = NT_VALUE;
    val_node->type.location = *location;
    val_node->value = value;

    return val_node;
}

VariableNode *cp_new_variablenode(const ParseLocation *location, const char *var_name)
{
    VariableNode *var_node = malloc(sizeof(VariableNode));
    RETURN_IF_NULL(var_node);

    var_node->type.node_type = NT_VARIABLE;
    var_node->type.location = *location;
    var_node->name = strdupe(var_name);

    if (var_node->name == NULL) {
        free(var_node);
        return NULL;
    }

    return var_node;
}

ExpressionNode *cp_new_expressionnode(const ParseLocation *location, ASTNodeType node_type, 
                                      ASTNode *left, ASTNode *right)
{
    ExpressionNode *exp_node = malloc(sizeof(ExpressionNode));
    RETURN_IF_NULL(exp_node);

    exp_node->type.node_type = node_type;
    exp_node->type.location = *location;
    exp_node->left = left;
    exp_node->right = right;

    return exp_node;
}

StatementNode *cp_new_statementnode(const ParseLocation *location, ASTNode *node)
{
    StatementNode *stm_node = malloc(sizeof(StatementNode));
    RETURN_IF_NULL(stm_node);

    stm_node->type.node_type = NT_STATEMENT;
    stm_node->type.location = *location;
    stm_node->node = node;
    stm_node->next = NULL;

    return stm_node;
}

StatementBlockNode *cp_new_statementblocknode(const ParseLocation *location, 
                                              const char *block_name, ASTNode *node)
{
    StatementBlockNode *stmb_node = malloc(sizeof(StatementBlockNode));
    RETURN_IF_NULL(stmb_node);

    stmb_node->type.node_type = NT_STATEMENT_BLOCK;
    stmb_node->type.location = *location;
    stmb_node->node = node;
    stmb_node->block_name = strdupe(block_name);

    if (stmb_node->block_name == NULL) {
        free(stmb_node);
        return NULL;
    }

    return stmb_node;
}

int cp_convert_to_bool_value(const char *svalue, Value *value)
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

int cp_convert_to_int_value(const char *svalue, Value *value)
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

int cp_convert_to_string_value(const char *svalue, Value *value)
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

        value->type = VAL_TYPE_STR;
        value->val.sval = processed;

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

int cp_convert_to_regex_value(const char *rvalue, Value *value)
{
    if (rvalue == NULL || value == NULL) {
        return 0;
    }

    size_t length = strlen(rvalue);
    char *processed;

    if (length <= 2) {
        processed = malloc(sizeof(char));

        if (processed == NULL) {
            return 0;
        }

        *processed = '\0';

        value->type = VAL_TYPE_STR;
        value->val.sval = processed;

        return 1;
    } 

    const char *iter = rvalue;
    size_t esc_count = 0;

    while (*iter++) {
        if (*iter == '\\' && *(iter + 1) == '/') {
            esc_count++;
            iter++;
        }
    }

    processed = malloc(sizeof(char) * (length - esc_count) - 1);

    if (processed == NULL) {
        return 0;
    }

    iter = rvalue;
    const char *end = rvalue + length - 1;
    char *proc = processed;

    while (++iter != end) {
        if (*iter == '\\' && *(iter + 1) == '/') {
            *proc++ = '/';
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

int cp_add_statement_to_list(ASTNode *statememt_list, ASTNode *statememt)
{
    assert(statememt_list != NULL);

    if (statememt_list == NULL || statememt == NULL) {
        return 0; 
    }

    assert(statememt_list->node_type == NT_STATEMENT);
    assert(statememt->node_type == NT_STATEMENT);

    if (statememt_list->node_type != NT_STATEMENT ||
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

int cp_eval_ast(Session *sess, ConfigLevel config_level, ASTNode *node)
{
    if (node == NULL) {
        return 0;
    }

    switch (node->node_type) {
        case NT_STATEMENT:
            {
                StatementNode *stm_node = (StatementNode *)node;

                while (stm_node != NULL) {
                    cp_eval_ast(sess, config_level, stm_node->node);
                    stm_node = stm_node->next;
                }

                break;
            }
        case NT_ASSIGNMENT:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                VariableNode *var_node = (VariableNode *)exp_node->left;
                ValueNode *val_node = (ValueNode *)exp_node->right;

                se_add_error(sess, cf_set_var(sess, config_level, var_node->name, val_node->value));
                break;
            }
        case NT_REFERENCE:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                VariableNode *var_node = (VariableNode *)exp_node->left;
                se_add_error(sess, cf_print_var(sess, var_node->name));
                break;
            }
        case NT_STATEMENT_BLOCK:
            {
                StatementBlockNode *stmb_node = (StatementBlockNode *)node;
                cp_process_block(sess, stmb_node);
            }
        default:
            return 0;
    }

    return 1;
}

void cp_free_ast(ASTNode *node)
{
    if (node == NULL) {
        return;
    }

    switch (node->node_type) {
        case NT_VALUE:
            {
                ValueNode *val_node = (ValueNode *)node;
                va_free_value(val_node->value);
                break;
            }
        case NT_VARIABLE:
            {
                VariableNode *var_node = (VariableNode *)node;
                free(var_node->name);
                break;
            }
        case NT_ASSIGNMENT:
        case NT_REFERENCE:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                cp_free_ast(exp_node->left);
                cp_free_ast(exp_node->right);
                break;
            }
        case NT_STATEMENT:
            {
                StatementNode *stm_node = (StatementNode *)node;
                StatementNode *last;

                while (stm_node != NULL) {
                    cp_free_ast(stm_node->node);
                    last = stm_node;
                    stm_node = stm_node->next;
                    free(last);
                }

                node = NULL;
                break;
            }
        case NT_STATEMENT_BLOCK:
            {
                StatementBlockNode *stmb_node = (StatementBlockNode *)node;
                free(stmb_node->block_name);
                cp_free_ast(stmb_node->node);
                break;
            }
        default:
            break;
    }

    if (node != NULL) {
        free(node);
    }
}

void cp_update_parser_location(const char *yytext, const char *file_name)
{
    yylloc.file_name = file_name;
    yylloc.first_line = yylloc.last_line; 
    yylloc.first_column = yylloc.last_column; 

    for (size_t k = 0; yytext[k] != '\0'; k++) { 
        if (yytext[k] == '\n') { 
            yylloc.last_line++; 
            yylloc.last_column = 1; 
        } else { 
            yylloc.last_column++; 
        } 
    }
}

static void cp_reset_parser_location(void)
{
    yylloc.first_line = yylloc.last_line = 1;
    yylloc.first_column = yylloc.last_column = 1;
}

Status cp_get_config_error(ErrorCode error_code, const ParseLocation *location, const char *format, ...)
{
    assert(!is_null_or_empty(format));

    va_list arg_ptr;
    va_start(arg_ptr, format);
    Status status;

    if (location->file_name != NULL) {
        char new_format[MAX_ERROR_MSG_SIZE];

        snprintf(new_format, MAX_ERROR_MSG_SIZE, "%s:%d:%d: %s", location->file_name, 
                 location->first_line, location->first_column, format);

        status = st_get_custom_error(error_code, new_format, arg_ptr);
    } else {
        status = st_get_custom_error(error_code, format, arg_ptr);
    }

    va_end(arg_ptr);

    return status;
}

void yyerror(Session *sess, ConfigLevel config_level, const char *file_name, char const *error)
{
    (void)config_level;
    (void)file_name;

    se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_SYNTAX, &yylloc, error));
}

Status cp_parse_config_file(Session *sess, ConfigLevel config_level, const char *config_file_path)
{
    assert(!is_null_or_empty(config_file_path));

    FILE *config_file = fopen(config_file_path, "rb");

    if (config_file == NULL) {
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE, 
                         "Unable to open file %s for reading", config_file_path);
    } 

    yyrestart(config_file);
    cp_reset_parser_location();

    int parse_status = yyparse(sess, config_level, config_file_path);

    if (parse_status != 0) {
        return st_get_error(ERR_FAILED_TO_PARSE_CONFIG_FILE, 
                         "Failed to fully config parse file %s", config_file_path);
    }

    return STATUS_SUCCESS;
}

Status cp_parse_config_string(Session *sess, ConfigLevel config_level, const char *str)
{
    assert(!is_null_or_empty(str));

    cp_start_scan_string(str);
    cp_reset_parser_location();

    int parse_status = yyparse(sess, config_level, NULL);

    cp_finish_scan_string();

    if (parse_status != 0) {
        return st_get_error(ERR_FAILED_TO_PARSE_CONFIG_INPUT, "Failed to fully parse config input");
    }

    return STATUS_SUCCESS;
}

static void cp_process_block(Session *sess, StatementBlockNode *stmb_node)
{
    if (!is_null_or_empty(stmb_node->block_name)) {
        if (strncmp(stmb_node->block_name, "filetype", 9) == 0) {
            cp_process_filetype_block(sess, stmb_node);
            return;
        } else if (strncmp(stmb_node->block_name, "syntax", 7) == 0) {
            return;
        }
    }

    se_add_error(sess, cp_get_config_error(ERR_INVALID_BLOCK_IDENTIFIER, 
                                           &stmb_node->type.location,
                                           "Invalid block identifier: \"%s\"",
                                           stmb_node->block_name));
}

static void cp_process_filetype_block(Session *sess, StatementBlockNode *stmb_node)
{
    if (stmb_node->node == NULL) {
        se_add_error(sess, cp_get_config_error(ERR_EMPTY_BLOCK_DEFINITION,
                                               &stmb_node->type.location,
                                               "Empty block definition"));
        return;
    } else if (stmb_node->node->node_type != NT_STATEMENT) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &stmb_node->node->location,
                                               "Invalid block entry"));
        return;
    }

    char *name = NULL;
    char *display_name = NULL;
    char *file_pattern = NULL;

    VariableAssignment expected_vars[] = {
        { "name"        , VAL_TYPE_STR, &name        , NULL, 0 },
        { "display_name", VAL_TYPE_STR, &display_name, NULL, 0 },
        { "file_pattern", VAL_TYPE_STR, &file_pattern, NULL, 0 }
    };

    size_t expected_vars_num = sizeof(expected_vars) / sizeof(VariableAssignment);

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            cp_process_assignment(sess, stm_node, expected_vars, expected_vars_num);
            stm_node = stm_node->next;
        } else {
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement in filetype block"));
            return;
        }
    }

    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num, 
                                &stmb_node->type.location, "filetype", 1)) {
        return;
    }

    FileType *file_type;
    Status status = ft_init(&file_type, name, display_name, file_pattern);

    if (!STATUS_IS_SUCCESS(status)) {
        se_add_error(sess, status);
        return;
    }

    se_add_error(sess, se_add_filetype_def(sess, file_type));
}

static int cp_process_assignment(Session *sess, StatementNode *stm_node,
                                 VariableAssignment *expected_vars, size_t expected_vars_num)
{
    assert(stm_node != NULL);

    if (stm_node->node == NULL || stm_node->node->node_type != NT_ASSIGNMENT) {
        return 0;
    }

    ExpressionNode *exp_node = (ExpressionNode *)stm_node->node;
    VariableNode *var_node = (VariableNode *)exp_node->left;
    ValueNode *val_node = (ValueNode *)exp_node->right;

    if (var_node == NULL || val_node == NULL) {
        return 0;
    }

    size_t found_var_idx = expected_vars_num;

    for (size_t k = 0; k < expected_vars_num; k++) {
        if (strcmp(var_node->name, expected_vars[k].var_name) == 0) {
            found_var_idx = k;
            break; 
        }
    }

    if (found_var_idx == expected_vars_num) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &var_node->type.location,
                                               "Invalid variable: %s",
                                               var_node->name));
        return 0;
    }

    if (expected_vars[found_var_idx].value_type != val_node->value.type) {
        const char *value_type = va_value_type_string(expected_vars[found_var_idx].value_type);

        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &var_node->type.location,
                                               "Invalid type, variable %s must have type %s",
                                               var_node->name,
                                               value_type));
        return 0;
    }

    if (val_node->value.type == VAL_TYPE_STR) {
        char **value = (char **)expected_vars[found_var_idx].value;
        *value = val_node->value.val.sval;
    }

    expected_vars[found_var_idx].location = &var_node->type.location;
    expected_vars[found_var_idx].value_set = 1;

    return 1;
}

static int cp_validate_block_vars(Session *sess, VariableAssignment *expected_vars, 
                                  size_t expected_vars_num, const ParseLocation *block_location,
                                  const char *block_name, int non_null_empty)
{
    int valid = 1;

    for (size_t k = 0; k < expected_vars_num; k++) {
        if (!expected_vars[k].value_set) {
            se_add_error(sess, cp_get_config_error(ERR_MISSING_VARIABLE_DEFINITION,
                                                   block_location,
                                                   "%s definition missing %s "
                                                   "variable assignment",
                                                   block_name, expected_vars[k].var_name));
            valid = 0;
        }
    }

    if (non_null_empty) {
        for (size_t k = 0; k < expected_vars_num; k++) {
            if (expected_vars[k].value_set && 
                expected_vars[k].value_type == VAL_TYPE_STR &&
                is_null_or_empty(*(char **)expected_vars[k].value)) {

                se_add_error(sess, cp_get_config_error(ERR_INVALID_VAL,
                            expected_vars[k].location,
                            "Invalid value \"%s\" for variable %s in %s defintion",
                            *(char **)expected_vars[k].value, 
                            expected_vars[k].var_name, block_name));
                valid = 0;
            }
        }
    }

    return valid;
}
