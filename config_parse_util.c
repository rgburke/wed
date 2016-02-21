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

/* For memrchr */
#define _GNU_SOURCE

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
#include "syntax.h"
#include "theme.h"

/* Bison v2.5 shipped with Ubuntu 12.04.5 LTS doesn't add a yyparse
 * declaration to config_parse.h, so we have to add it here to avoid an
 * implicit function declaration warning */
int yyparse (Session *sess, ConfigLevel config_level, const char *file_path);

/* Used to process variable assignments in wed block definitions.
 * i.e. A list of expected variable assignments can be used to
 * validate a block definition */
typedef struct {
    char *var_name; /* Variable name */
    ValueType value_type; /* Expected type for this variable */
    Value *value; /* This is set to the value found */
    const ParseLocation *location; /* This is set to the location
                                      of the assignment */
    int value_set; /* True if this variable was found and its value set */
} VariableAssignment;

static void cp_reset_parser_location(void);
static void cp_process_block(Session *, StatementBlockNode *);
static int cp_basic_block_check(Session *, StatementBlockNode *);
static void cp_process_filetype_block(Session *, StatementBlockNode *);
static void cp_process_syntax_block(Session *, StatementBlockNode *);
static SyntaxPattern *cp_process_syntax_pattern_block(Session *,
                                                      StatementBlockNode *);
static void cp_process_theme_block(Session *, StatementBlockNode *);
static void cp_process_theme_group_block(Session *, Theme *,
                                         StatementBlockNode *);
static int cp_process_assignment(Session *, StatementNode *,
                                 VariableAssignment *,
                                 size_t expected_vars_num);
static int cp_validate_block_vars(Session *, VariableAssignment *,
                                  size_t expected_vars_num,
                                  const ParseLocation *,
                                  const char *block_name, int non_null_empty);
static ConfigLevel cp_determine_config_level(const char *var_name, ConfigLevel);

ValueNode *cp_new_valuenode(const ParseLocation *location, Value value)
{
    ValueNode *val_node = malloc(sizeof(ValueNode));
    RETURN_IF_NULL(val_node);

    val_node->type.node_type = NT_VALUE;
    val_node->type.location = *location;
    val_node->value = value;

    return val_node;
}

VariableNode *cp_new_variablenode(const ParseLocation *location,
                                  const char *var_name)
{
    VariableNode *var_node = malloc(sizeof(VariableNode));
    RETURN_IF_NULL(var_node);

    var_node->type.node_type = NT_VARIABLE;
    var_node->type.location = *location;
    var_node->name = strdup(var_name);

    if (var_node->name == NULL) {
        free(var_node);
        return NULL;
    }

    return var_node;
}

ExpressionNode *cp_new_expressionnode(const ParseLocation *location,
                                      ASTNodeType node_type,
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

StatementNode *cp_new_statementnode(const ParseLocation *location,
                                    ASTNode *node)
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
                                              const char *block_name,
                                              ASTNode *node)
{
    StatementBlockNode *stmb_node = malloc(sizeof(StatementBlockNode));
    RETURN_IF_NULL(stmb_node);

    stmb_node->type.node_type = NT_STATEMENT_BLOCK;
    stmb_node->type.location = *location;
    stmb_node->node = node;
    stmb_node->block_name = strdup(block_name);

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
    
    int bool_val;

    if (strncmp(svalue, "true", 5) == 0 ||
        strncmp(svalue, "1", 2) == 0) {
        bool_val = 1;
    } else if (strncmp(svalue, "false", 6) == 0 ||
               strncmp(svalue, "0", 2) == 0) {
        bool_val = 0;
    } else {
        return 0;
    }

    *value = BOOL_VAL(bool_val);

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

    /* Check strtol was successful. See man strtol for more details */
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
        (errno != 0 && val == 0) || end_ptr == svalue) {
        return 0;
    }

    *value = INT_VAL(val);

    return 1;
}

int cp_convert_to_string_value(const char *svalue, Value *value)
{
    if (svalue == NULL || value == NULL) {
        return 0;
    }

    size_t length = strlen(svalue);

    if (length < 2 || svalue[0] != '"' || 
        svalue[length - 1] != '"') {
        return 0;
    }

    char *processed;

    /* Empty string "" */
    if (length == 2) {
        processed = malloc(sizeof(char));

        if (processed == NULL) {
            return 0;
        }

        *processed = '\0';

        *value = STR_VAL(processed);

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

    /* We will convert the escaped characters into the
     * actual characters they represent */
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

    *value = STR_VAL(processed);

    return 1;
}

int cp_convert_to_regex_value(const char *rvalue, Value *value)
{
    if (rvalue == NULL || value == NULL) {
        return 0;
    }

    size_t length = strlen(rvalue);

    if (length < 2 || rvalue[0] != '/') {
        return 0;
    }

    /* Find terminating / character. It's not the last character
     * in the string as modifiers can be specified */
    const char *regex_end = memrchr(rvalue + 1, '/', length - 1);

    if (regex_end == NULL) {
        return 0;
    }

    char *processed;

    if (length == 2 || rvalue[1] == '/') {
        processed = malloc(sizeof(char));

        if (processed == NULL) {
            return 0;
        }

        *processed = '\0';

        *value = REGEX_VAL(processed, 0);

        return 1;
    } 

    const char *iter = rvalue;
    size_t esc_count = 0;

    while (++iter != regex_end) {
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
    char *proc = processed;

    while (++iter != regex_end) {
        if (*iter == '\\' && *(iter + 1) == '/') {
            *proc++ = '/';
            iter++;
        } else {
            *proc++ = *iter;
        }    
    }

    *proc = '\0';

    int modifiers = 0;
    iter = regex_end;

    while (*++iter != '\0') {
        switch (*iter) {
            case 'i': 
                {
                    modifiers |= PCRE_CASELESS;
                    break;
                }
            case 'x':
                {
                    modifiers |= PCRE_EXTENDED;
                    break;
                }
            case 's':
                {
                    modifiers |= PCRE_DOTALL;
                    break;
                }
            case 'm':
                {
                    modifiers |= PCRE_MULTILINE;
                    break;
                }
            default:
                {
                    /* TODO Error on invalid flags */
                    break;
                }
        } 
    }

    *value = REGEX_VAL(processed, modifiers);

    return 1;
}

/* Add statement onto the end of statement linked list */
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

                /* When at CL_BUFFER allow setting purely CL_SESSION
                 * variables as there is no ambiguity. This allows us to
                 * set session level vars such as theme in wed's command
                 * prompt which is at the CL_BUFFER level */
                ConfigLevel tmp_config_level =
                                cp_determine_config_level(var_node->name,
                                                          config_level);

                Status status = cf_set_named_var(
                                    CE_VAL(sess, sess->active_buffer),
                                    tmp_config_level,
                                    var_node->name, val_node->value);

                se_add_error(sess, cp_convert_to_config_error(status,
                                                              &node->location));

                break;
            }
        case NT_REFERENCE:
            {
                ExpressionNode *exp_node = (ExpressionNode *)node;
                VariableNode *var_node = (VariableNode *)exp_node->left;

                ConfigLevel tmp_config_level =
                                cp_determine_config_level(var_node->name,
                                                          config_level);

                Status status = cf_print_var(CE_VAL(sess, sess->active_buffer),
                                             tmp_config_level, var_node->name);

                se_add_error(sess, cp_convert_to_config_error(status,
                                                              &node->location));

                break;
            }
        case NT_STATEMENT_BLOCK:
            {
                StatementBlockNode *stmb_node = (StatementBlockNode *)node;
                cp_process_block(sess, stmb_node);

                break;
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

/* This function is invoked by yylex via the YY_USER_ACTION macro,
 * before a matched rules action is executed. Here we use this event
 * to update yyloc to the position of the just matched token */
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

/* Reset position before parsing a new file or string */
static void cp_reset_parser_location(void)
{
    yylloc.first_line = yylloc.last_line = 1;
    yylloc.first_column = yylloc.last_column = 1;
}

Status cp_convert_to_config_error(Status error, const ParseLocation *location)
{
    if (STATUS_IS_SUCCESS(error)) {
        return error;
    }

    Status config_error = cp_get_config_error(error.error_code, location,
                                              error.msg);

    st_free_status(error);

    return config_error;
}

/* Add location information to error message */
Status cp_get_config_error(ErrorCode error_code, const ParseLocation *location,
                           const char *format, ...)
{
    assert(!is_null_or_empty(format));

    va_list arg_ptr;
    va_start(arg_ptr, format);
    Status status;

    if (location->file_name != NULL) {
        char new_format[MAX_ERROR_MSG_SIZE];

        snprintf(new_format, MAX_ERROR_MSG_SIZE, "%s:%d:%d: %s",
                 location->file_name, location->first_line,
                 location->first_column, format);

        status = st_get_custom_error(error_code, new_format, arg_ptr);
    } else {
        status = st_get_custom_error(error_code, format, arg_ptr);
    }

    va_end(arg_ptr);

    return status;
}

/* Called when Bison encounters a syntax error */
void yyerror(Session *sess, ConfigLevel config_level, const char *file_name,
             char const *error)
{
    (void)config_level;
    (void)file_name;

    se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_SYNTAX,
                                           &yylloc, error));
}

Status cp_parse_config_file(Session *sess, ConfigLevel config_level,
                            const char *config_file_path)
{
    assert(!is_null_or_empty(config_file_path));

    FILE *config_file = fopen(config_file_path, "rb");

    if (config_file == NULL) {
        return st_get_error(ERR_UNABLE_TO_OPEN_FILE,
                            "Unable to open file %s for reading",
                            config_file_path);
    } 

    cp_start_scan_file(sess->cfg_buffer_stack, config_file);
    cp_reset_parser_location();

    /* Invoke Bison parser */
    int parse_status = yyparse(sess, config_level, config_file_path);

    cp_finish_scan(sess->cfg_buffer_stack);

    if (parse_status != 0) {
        return st_get_error(ERR_FAILED_TO_PARSE_CONFIG_FILE, 
                            "Failed to fully config parse file %s",
                            config_file_path);
    }

    return STATUS_SUCCESS;
}

Status cp_parse_config_string(Session *sess, ConfigLevel config_level,
                              const char *str)
{
    assert(!is_null_or_empty(str));

    cp_start_scan_string(sess->cfg_buffer_stack, str);
    cp_reset_parser_location();

    int parse_status = yyparse(sess, config_level, NULL);

    cp_finish_scan(sess->cfg_buffer_stack);

    if (parse_status != 0) {
        return st_get_error(ERR_FAILED_TO_PARSE_CONFIG_INPUT,
                            "Failed to fully parse config input");
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
            cp_process_syntax_block(sess, stmb_node);
            return;
        } else if (strncmp(stmb_node->block_name, "theme", 6) == 0) {
            cp_process_theme_block(sess, stmb_node);
            return;
        }
    }

    se_add_error(sess, cp_get_config_error(ERR_INVALID_BLOCK_IDENTIFIER,
                                           &stmb_node->type.location,
                                           "Invalid block identifier: \"%s\"",
                                           stmb_node->block_name));
}

/* Basic check performed for all block definitions */
static int cp_basic_block_check(Session *sess, StatementBlockNode *stmb_node)
{
    if (stmb_node->node == NULL) {
        se_add_error(sess, cp_get_config_error(ERR_EMPTY_BLOCK_DEFINITION,
                                               &stmb_node->type.location,
                                               "Empty block definition"));
        return 0;
    } else if (stmb_node->node->node_type != NT_STATEMENT) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &stmb_node->node->location,
                                               "Invalid block entry"));
        return 0;
    }

    return 1;
}

static void cp_process_filetype_block(Session *sess,
                                      StatementBlockNode *stmb_node)
{
    if (!cp_basic_block_check(sess, stmb_node)) {
        return;
    }

    Value name;
    Value display_name;
    Value file_pattern;

    /* Variable assignements expected in a filetype block */
    VariableAssignment expected_vars[] = {
        { "name"        , VAL_TYPE_STR  , &name        , NULL, 0 },
        { "display_name", VAL_TYPE_STR  , &display_name, NULL, 0 },
        { "file_pattern", VAL_TYPE_REGEX, &file_pattern, NULL, 0 }
    };

    size_t expected_vars_num =
        ARRAY_SIZE(expected_vars, VariableAssignment);

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            /* Process the assignment we've found and update
             * the relevant expected variable value if valid */
            cp_process_assignment(sess, stm_node, expected_vars,
                                  expected_vars_num);
            stm_node = stm_node->next;
        } else {
            /* There should only be assignments in a filetype block */
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement "
                                                   "in filetype block"));
            return;
        }
    }

    /* Validate all expected values have been set */
    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num, 
                                &stmb_node->type.location, "filetype", 1)) {
        return;
    }

    FileType *file_type;
    Status status = ft_init(&file_type, SVAL(name), SVAL(display_name),
                            &RVAL(file_pattern));

    if (!STATUS_IS_SUCCESS(status)) {
        se_add_error(sess, status);
        return;
    }

    /* Session keeps track of all loaded filetypes */
    status = se_add_filetype_def(sess, file_type);

    if (!STATUS_IS_SUCCESS(status)) {
        ft_free(file_type);
        se_add_error(sess, status);
    }
}

static void cp_process_syntax_block(Session *sess,
                                    StatementBlockNode *stmb_node)
{
    if (!cp_basic_block_check(sess, stmb_node)) {
        return;
    }

    Value name;

    VariableAssignment expected_vars[] = {
        { "name", VAL_TYPE_STR, &name, NULL, 0 }
    };

    const size_t expected_vars_num =
        ARRAY_SIZE(expected_vars, VariableAssignment);

    SyntaxDefinition *syn_def = NULL;
    SyntaxPattern *syn_current = NULL;
    SyntaxPattern *syn_first = NULL;

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            cp_process_assignment(sess, stm_node, expected_vars,
                                  expected_vars_num);
            stm_node = stm_node->next;
        } else if (stm_node->node->node_type == NT_STATEMENT_BLOCK) {
            SyntaxPattern *syn_pattern = cp_process_syntax_pattern_block(sess, 
                                         (StatementBlockNode *)stm_node->node);

            if (syn_pattern != NULL) {
                /* Build linked list of SyntaxPattern's */
                if (syn_first == NULL) {
                    syn_first = syn_current = syn_pattern; 
                } else {
                    syn_current->next = syn_pattern;
                    syn_current = syn_pattern;
                }
            }

            stm_node = stm_node->next;
        } else {
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement in "
                                                   "syntax block"));
            goto cleanup;
        }
    }

    if (syn_first == NULL) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &stmb_node->type.location,
                                               "Synax block contains no "
                                               "valid pattern blocks"));
        return;
    }

    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num,
                                &stmb_node->type.location, "syntax", 1)) {
        goto cleanup;
    }

    syn_def = sy_new_def(syn_first);

    if (syn_def == NULL) {
        se_add_error(sess, st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                        "Unable to allocate SyntaxDefinition"));

        goto cleanup;
    }

    Status status = se_add_syn_def(sess, syn_def, SVAL(name));

    if (!STATUS_IS_SUCCESS(status)) {
        se_add_error(sess, status);
        goto cleanup;
    }

    return;

cleanup:

    if (syn_def == NULL) {
        while (syn_first != NULL) {
            syn_current = syn_first->next;
            syn_free_pattern(syn_first);
            syn_first = syn_current;
        }
    } else {
        sy_free_def(syn_def);
    }
}

static SyntaxPattern *cp_process_syntax_pattern_block(Session *sess, 
                                                  StatementBlockNode *stmb_node)
{
    if (!cp_basic_block_check(sess, stmb_node)) {
        return NULL;
    }

    Value regex;
    Value type;

    VariableAssignment expected_vars[] = {
        { "regex", VAL_TYPE_REGEX, &regex, NULL, 0 },
        { "type" , VAL_TYPE_STR  , &type , NULL, 0 }
    };

    size_t expected_vars_num =
        ARRAY_SIZE(expected_vars, VariableAssignment);

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            cp_process_assignment(sess, stm_node, expected_vars,
                                  expected_vars_num);
            stm_node = stm_node->next;
        } else {
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement in "
                                                   "pattern block"));
            return NULL;
        }
    }

    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num, 
                                &stmb_node->type.location, "pattern", 1)) {
        return NULL;
    }

    SyntaxToken token;

    if (!sy_str_to_token(&token, SVAL(type))) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               expected_vars[1].location,
                                               "Invalid type \"%s\" in "
                                               "pattern block",
                                               SVAL(type)));
        return NULL;
    }

    SyntaxPattern *syn_pattern;
    Status status = sy_new_pattern(&syn_pattern, &RVAL(regex), token);

    if (!STATUS_IS_SUCCESS(status)) {
        se_add_error(sess, cp_get_config_error(status.error_code, 
                                               &stmb_node->type.location,
                                               "Invalid pattern block - %s",
                                               status.msg));
        st_free_status(status);
        return NULL;
    }

    return syn_pattern;
}

static void cp_process_theme_block(Session *sess, StatementBlockNode *stmb_node)
{
    if (!cp_basic_block_check(sess, stmb_node)) {
        return;
    }

    Value name;

    VariableAssignment expected_vars[] = {
        { "name", VAL_TYPE_STR, &name, NULL, 0 }
    };

    const size_t expected_vars_num =
        ARRAY_SIZE(expected_vars, VariableAssignment);

    /* theme blocks extend the default theme. That is, the default
     * theme is used as a base which any other theme definition can
     * override. This ensures that all necessary components have colors
     * specified for them and users can simply modify the components they're
     * interested in without having to specify colors for all components */
    Theme *theme = th_get_default_theme();

    if (theme == NULL) {
        se_add_error(sess, st_get_error(ERR_OUT_OF_MEMORY,
                                        "Out Of Memory - ",
                                        "Unable to create Theme"));
        return;
    }

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            cp_process_assignment(sess, stm_node, expected_vars,
                                  expected_vars_num);
            stm_node = stm_node->next;
        } else if (stm_node->node->node_type == NT_STATEMENT_BLOCK) {
            cp_process_theme_group_block(sess, theme,
                                         (StatementBlockNode *)stm_node->node);
            stm_node = stm_node->next;
        } else {
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement in "
                                                   "theme block"));
            return;
        }
    }

    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num, 
                                &stmb_node->type.location, "theme", 1)) {
    }

    se_add_error(sess, se_add_theme(sess, theme, SVAL(name)));
}

static void cp_process_theme_group_block(Session *sess, Theme *theme,
                                         StatementBlockNode *stmb_node)
{
    if (!cp_basic_block_check(sess, stmb_node)) {
        return;
    }

    Value name, fg_color_val, bg_color_val;

    VariableAssignment expected_vars[] = {
        { "name"    , VAL_TYPE_STR, &name        , NULL, 0 },
        { "fgcolor" , VAL_TYPE_STR, &fg_color_val, NULL, 0 },
        { "bgcolor" , VAL_TYPE_STR, &bg_color_val, NULL, 0 }
    };

    size_t expected_vars_num =
        ARRAY_SIZE(expected_vars, VariableAssignment);

    StatementNode *stm_node = (StatementNode *)stmb_node->node;

    while (stm_node != NULL) {
        if (stm_node->node->node_type == NT_ASSIGNMENT) {
            cp_process_assignment(sess, stm_node, expected_vars,
                                  expected_vars_num);
            stm_node = stm_node->next;
        } else {
            se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                                   &stm_node->node->location,
                                                   "Invalid statement in "
                                                   "group block"));
            return;
        }
    }

    if (!cp_validate_block_vars(sess, expected_vars, expected_vars_num, 
                                &stmb_node->type.location, "group", 1)) {
        return;
    }

    DrawColor fg_color, bg_color;
    int valid_def = 1;

    if (!th_str_to_draw_color(&fg_color, SVAL(fg_color_val))) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               expected_vars[1].location,
                                               "Invalid fgcolor \"%s\" "
                                               "in group block",
                                               SVAL(fg_color_val)));
        valid_def = 0;
    } 

    if (!th_str_to_draw_color(&bg_color, SVAL(bg_color_val))) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               expected_vars[2].location,
                                               "Invalid bgcolor \"%s\" "
                                               "in group block",
                                               SVAL(bg_color_val)));
        valid_def = 0;
    }

    if (!th_is_valid_group_name(SVAL(name))) {
        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               expected_vars[0].location,
                                               "Invalid group name \"%s\" "
                                               "in group block",
                                               SVAL(name)));
        valid_def = 0;
    }

    if (!valid_def) {
        return;
    }

    SyntaxToken token;
    ScreenComponent screen_comp;

    if (sy_str_to_token(&token, SVAL(name))) {
        th_set_screen_comp_colors(theme, token, fg_color, bg_color);        
    } else if (th_str_to_screen_component(&screen_comp, SVAL(name))) {
        th_set_screen_comp_colors(theme, screen_comp, fg_color, bg_color);        
    }
}

static int cp_process_assignment(Session *sess, StatementNode *stm_node,
                                 VariableAssignment *expected_vars,
                                 size_t expected_vars_num)
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

    VariableAssignment *var_asn = &expected_vars[found_var_idx];

    if (var_asn->value_type != val_node->value.type) {
        const char *value_type = va_value_type_string(var_asn->value_type);

        se_add_error(sess, cp_get_config_error(ERR_INVALID_CONFIG_ENTRY,
                                               &var_node->type.location,
                                               "Invalid type, variable %s "
                                               "must have type %s",
                                               var_node->name,
                                               value_type));
        return 0;
    }

    *var_asn->value = val_node->value;
    var_asn->location = &var_node->type.location;
    var_asn->value_set = 1;

    return 1;
}

static int cp_validate_block_vars(Session *sess,
                                  VariableAssignment *expected_vars,
                                  size_t expected_vars_num,
                                  const ParseLocation *block_location,
                                  const char *block_name, int non_null_empty)
{
    int valid = 1;

    /* Check all expected values are set */
    for (size_t k = 0; k < expected_vars_num; k++) {
        if (!expected_vars[k].value_set) {
            se_add_error(sess,
                         cp_get_config_error(ERR_MISSING_VARIABLE_DEFINITION,
                                             block_location,
                                             "%s definition missing %s "
                                             "variable assignment",
                                             block_name,
                                             expected_vars[k].var_name));
            valid = 0;
        }
    }

    if (non_null_empty) {
        /* Check any string or regex values aren't NULL or empty */
        for (size_t k = 0; k < expected_vars_num; k++) {
            if (!expected_vars[k].value_set || 
                !STR_BASED_VAL(*expected_vars[k].value)) {
                continue; 
            }

            const char *val = va_str_val(*expected_vars[k].value);

            if (is_null_or_empty(val)) {
                se_add_error(sess, cp_get_config_error(ERR_INVALID_VAL,
                             expected_vars[k].location,
                             "Invalid value \"%s\" for variable %s "
                             "in %s defintion",
                             val, expected_vars[k].var_name, block_name));
                valid = 0;
            }
        }
    }

    return valid;
}

/* Allow CL_SESSION level only vars to be set at CL_BUFFER
 * as there is no ambiguity over the ConfigLevel */
static ConfigLevel cp_determine_config_level(const char *var_name,
                                             ConfigLevel config_level)
{
    ConfigVariable config_variable;

    if (config_level == CL_BUFFER &&
        cf_str_to_var(var_name, &config_variable) &&
        cf_get_config_levels(config_variable) == CL_SESSION) {
        config_level = CL_SESSION;
    }

    return config_level;
}
