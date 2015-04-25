/*
 * Copyright (C) 2014 Richard Burke
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include "config.h"
#include "value.h"
#include "hashmap.h"
#include "util.h"
#include "command.h"
#include "buffer.h"
#include "config_parse_util.h"
#include "config_parse.h"

#define CFG_LINE_ALLOC 512
#define CFG_FILE_NAME "wedrc"
#define CFG_SYSTEM_DIR "/etc"

#define CFG_TABWIDTH_MIN 1
#define CFG_TABWIDTH_MAX 8

static Session *curr_sess = NULL;
static HashMap *config_vars = NULL;

static int is_valid_var(const char *);
static ConfigVariableDescriptor *clone_config_var_descriptor(const char *);
static int populate_default_config(HashMap *);
static const ConfigVariableDescriptor *get_variable(const char *);
static Status set_config_var(HashMap *, char *, Value);
static Status tabwidth_validator(Value);
static Status on_tabwidth_change(Session *, Value, Value);

static const ConfigVariableDescriptor default_config[] = {
    { "linewrap", "lw", BOOL_VAL_STRUCT(1), NULL              , NULL               },
    { "lineno"  , "ln", BOOL_VAL_STRUCT(1), NULL              , NULL               },
    { "tabwidth", "tw", INT_VAL_STRUCT(8) , tabwidth_validator, on_tabwidth_change }
};

void set_config_session(Session *sess)
{
    curr_sess = sess;
}

Status init_config(void)
{
    size_t var_num = sizeof(default_config) / sizeof(ConfigVariableDescriptor);
    config_vars = new_sized_hashmap(var_num * 4);

    if (!populate_default_config(config_vars)) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to load config");
    }

    return STATUS_SUCCESS;
}

void end_config(void)
{
    free_config(config_vars); 
}

static int is_valid_var(const char *var_name)
{
    return hashmap_get(config_vars, var_name) != NULL;
}

static ConfigVariableDescriptor *clone_config_var_descriptor(const char *var_name)
{
    ConfigVariableDescriptor *var = hashmap_get(config_vars, var_name);
    ConfigVariableDescriptor *clone = malloc(sizeof(ConfigVariableDescriptor));
    RETURN_IF_NULL(clone);
    memcpy(clone, var, sizeof(ConfigVariableDescriptor));

    if (!STATUS_IS_SUCCESS(deep_copy_value(clone->default_value, &clone->default_value))) {
        free(clone);
        return NULL;
    }

    return clone;
}

Status init_session_config(Session *sess)
{
    HashMap *config = sess->config;

    if (config == NULL) {
        size_t var_num = sizeof(default_config) / sizeof(ConfigVariableDescriptor);
        config = sess->config = new_sized_hashmap(var_num * 4);
    }

    if (!populate_default_config(config)) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to load config");
    }
    
    char *system_config_path = CFG_SYSTEM_DIR "/" CFG_FILE_NAME;

    if (access(system_config_path, F_OK) != -1) {
        add_error(sess, load_config(sess, system_config_path));
    }

    Status status = STATUS_SUCCESS;

    char *home_path = getenv("HOME"); 

    if (home_path != NULL) {
        size_t user_config_path_size = strlen(home_path) + strlen("/." CFG_FILE_NAME) + 1;
        char *user_config_path = malloc(user_config_path_size);

        if (user_config_path == NULL) {
            return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to check for config file in HOME directory"); 
        }

        snprintf(user_config_path, user_config_path_size, "%s/.%s", home_path, CFG_FILE_NAME);
        *(user_config_path + user_config_path_size - 1) = '\0';

        if (access(user_config_path, F_OK) != -1) {
            status = load_config(sess, user_config_path);
        }

        free(user_config_path);
    }

    return status;
}

void free_config(HashMap *config)
{
    if (config == NULL) {
        return;
    }

    size_t var_num = sizeof(default_config) / sizeof(ConfigVariableDescriptor);

    /* TODO Consider iterating over the key set of the hash instead */
    for (size_t k = 0; k < var_num; k++) {
        ConfigVariableDescriptor *cvd = hashmap_get(config, default_config[k].name);

        if (cvd == NULL) {
            continue;
        }

        free_value(cvd->default_value);
        free(cvd);
    }

    free_hashmap(config);
}

static int populate_default_config(HashMap *config)
{
    size_t var_num = sizeof(default_config) / sizeof(ConfigVariableDescriptor);
    ConfigVariableDescriptor *clone;

    for (size_t k = 0; k < var_num; k++) {
        clone = malloc(sizeof(ConfigVariableDescriptor));

        if (clone == NULL) {
            return 0;
        }

        memcpy(clone, &default_config[k], sizeof(ConfigVariableDescriptor));

        if (!STATUS_IS_SUCCESS(deep_copy_value(clone->default_value, &clone->default_value))) {
            return 0;
        }

        if (!(hashmap_set(config, clone->name, clone) && 
              hashmap_set(config, clone->short_name, clone))) {
            return 0;
        }
    }

    return 1;
}

Status load_config(Session *sess, char *config_file_path)
{
    return parse_config_file(sess, CL_SESSION, config_file_path);
}

Status set_var(Session *sess, ConfigLevel config_level, char *var_name, Value value)
{
    if (config_level == CL_SESSION) {
        return set_session_var(sess, var_name, value);
    }

    return set_buffer_var(sess->active_buffer, var_name, value);
}

Status set_session_var(Session *sess, char *var_name, Value value)
{
    return set_config_var(sess->config, var_name, value);
}

Status set_buffer_var(Buffer *buffer, char *var_name, Value value)
{
    return set_config_var(buffer->config, var_name, value);
}

static Status set_config_var(HashMap *config, char *var_name, Value value)
{
    if (config == NULL || var_name == NULL) {
        return get_error(ERR_INVALID_VAR, "Invalid property");
    }

    if (!is_valid_var(var_name)) {
        return get_error(ERR_INVALID_VAR, "Invalid property %s", var_name);
    }

    ConfigVariableDescriptor *var = hashmap_get(config, var_name);
    int existing_var = (var != NULL);

    if (!existing_var) {
        var = clone_config_var_descriptor(var_name);

        if (var == NULL) {
            return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to set config value");
        }
    }

    if (var->default_value.type != value.type) {
        if (var->default_value.type == VAL_TYPE_BOOL && value.type == VAL_TYPE_INT) {
            value.type = VAL_TYPE_BOOL; 
            value.val.ival = value.val.ival ? 1 : 0;
        } else {
            return get_error(ERR_INVALID_VAL, "%s must have value of type %s", 
                             var->name, get_value_type(var->default_value));
        }
    }

    if (var->custom_validator != NULL) {
        RETURN_IF_FAIL(var->custom_validator(value));
    }

    Value old_value = var->default_value;
    var->default_value = value; 

    if (!existing_var) {
        hashmap_set(config, var->name, var);
        hashmap_set(config, var->short_name, var);
    }

    Status status = STATUS_SUCCESS;

    if (var->on_change_event != NULL) {
        status = var->on_change_event(curr_sess, old_value, value);
    }

    free_value(old_value);

    char config_msg[MAX_MSG_SIZE];
    char *value_str = to_string(value);
    snprintf(config_msg, MAX_MSG_SIZE, "Set %s=%s", var->name, value_str);
    free(value_str);
    add_msg(curr_sess, config_msg);

    return status;
}

static const ConfigVariableDescriptor *get_variable(const char *var_name)
{
    Buffer *buffer = curr_sess->active_buffer;

    ConfigVariableDescriptor *var = NULL;
   
    if (buffer != NULL) {
        var = hashmap_get(buffer->config, var_name);
    }

    if (var == NULL) {
        var = hashmap_get(curr_sess->config, var_name);
    }

    return var;
}

Status print_var(Session *sess, const char *var_name)
{
    if (var_name == NULL) {
        return get_error(ERR_INVALID_VAR, "Invalid property");
    } else if (!is_valid_var(var_name)) {
        return get_error(ERR_INVALID_VAR, "Invalid property %s", var_name);
    }

    const ConfigVariableDescriptor *var = get_variable(var_name);

    char var_msg[MAX_MSG_SIZE];
    char *value_str = to_string(var->default_value);
    snprintf(var_msg, MAX_MSG_SIZE, "%s=%s", var->name, value_str);
    free(value_str);
    add_msg(sess, var_msg);

    return STATUS_SUCCESS;
}

int config_bool(char *var_name)
{
    const ConfigVariableDescriptor *var = get_variable(var_name);

    if (var == NULL || var->default_value.type != VAL_TYPE_BOOL) {
        /* TODO Add error to session error queue */
        return 0;
    }

    return var->default_value.val.ival;
}

long config_int(char *var_name)
{
    const ConfigVariableDescriptor *var = get_variable(var_name);

    if (var == NULL || var->default_value.type != VAL_TYPE_INT) {
        /* TODO Add error to session error queue */
        return 0;
    }

    return var->default_value.val.ival;
}

static Status tabwidth_validator(Value value)
{
    if (value.val.ival < CFG_TABWIDTH_MIN || value.val.ival > CFG_TABWIDTH_MAX) {
        return get_error(ERR_INVALID_TABWIDTH, "tabwidth value must be in range %d - %d inclusive",
                         CFG_TABWIDTH_MIN, CFG_TABWIDTH_MAX);
    }

    return STATUS_SUCCESS;
}

static Status on_tabwidth_change(Session *sess, Value old_value, Value new_value)
{
    (void)sess;

    if (old_value.val.ival == new_value.val.ival) {
        return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

