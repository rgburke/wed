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

#include "wed.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "config.h"
#include "variable.h"
#include "hashmap.h"
#include "util.h"
#include "command.h"
#include "buffer.h"

#define CFG_LINE_ALLOC 512
#define CFG_FILE_NAME "wedrc"
#define CFG_SYSTEM_DIR "/etc"

static Session *curr_sess = NULL;
static HashMap *config_vars = NULL;

static int is_valid_var(const char *);
static ConfigVariableDescriptor *clone_config_var_descriptor(const char *);
static int populate_default_config(HashMap *);
static char *get_config_line(FILE *);
static int process_config_line(char *, char **, char **);
static int get_bool_value(char *, Value *);
static Status set_config_var(HashMap *, char *, char *);

static int (*conversion_functions[])(char *, Value *) = {
    get_bool_value
};

static const ConfigVariableDescriptor default_config[] = {
    { "linewrap", "lw", BOOL_VAL_STRUCT(1), NULL, NULL }
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
        RETURN_IF_FAIL(load_config(sess, system_config_path));
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

    for (size_t k = 0; k < var_num; k++, clone++) {
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
    FILE *config_file = fopen(config_file_path, "rb");

    if (config_file == NULL) {
        return get_error(ERR_UNABLE_TO_OPEN_FILE, "Unable to open file %s for reading", config_file_path);
    } 

    Status status = STATUS_SUCCESS;
    size_t line_no = 0;
    char *line, *var, *val;

    while (!feof(config_file)) {
        line = get_config_line(config_file);

        if (line == NULL) {
            status = get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to read config file %s", config_file_path);
            break;
        }

        if (ferror(config_file)) {
            free(line);
            status = get_error(ERR_UNABLE_TO_READ_FILE, "Unable to read file %s", config_file_path);
            break;
        } 

        line_no++;

        if (!process_config_line(line, &var, &val)) {
            free(line);
            continue;
        }

        status = set_session_var(sess, var, val);

        free(line);

        if (!STATUS_IS_SUCCESS(status)) {
            Status error = get_error(ERR_INVALID_CONFIG_ENTRY, "%s on line %zu: %s", 
                                     config_file_path, line_no, status.error_msg);
            free_error(status);
            status = error;
            break;
        }
    }

    fclose(config_file);

    return status;
}

static char *get_config_line(FILE *file)
{
    size_t allocated = CFG_LINE_ALLOC;
    char *line = malloc(allocated);
    RETURN_IF_NULL(line);
    size_t line_size = 0;
    char *iter = line;
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (line_size++ == allocated) {
            line = realloc(line, allocated *= 2); 
            iter = line + line_size - 1;
        }
        if (c == '\n') {
            break;
        }

        *iter++ = c;
    }

    *iter = '\0';

    return line;
}

static int process_config_line(char *line, char **var, char **val)
{
    char *c = line;

    while (*c) {
        if (*c == '#' || *c == ';') {
            return 0;
        } else if (!isspace(*c)) {
            break;
        }

        c++;
    }

    if (*c) {
        *var = c;
    } else {
        return 0;
    }

    while (*c) {
        if (isspace(*c)) {
            *c++ = '\0';
            continue;
        } else if (*c == '=') {
            break; 
        }

        c++;
    }

    if (!(*c && *c == '=')) {
        return 0;
    } else {
        *c++ = '\0';
    }

    while (*c && isspace(*c)) {
        c++;
    }

    if (*c) {
        *val = c;
    } else {
        return 0;
    }

    char *last_space = NULL;
   
    while (*(++c)) {
        if (isspace(*c)) {
            if (last_space == NULL) {
                last_space = c;
            }
        } else {
            last_space = NULL;
        }
    } 

    if (last_space != NULL) {
        *last_space = '\0';
    }

    return 1;
}

static int get_bool_value(char *svalue, Value *value)
{
    if (svalue == NULL || value == NULL) {
        return 0;
    }

    value->type = VAL_TYPE_BOOL;

    if (strncmp(svalue, "true", 5) == 0 || strncmp(svalue, "1", 2) == 0) {
        value->val.ival = 1;
    } else if (strncmp(svalue, "false", 6) == 0 || strncmp(svalue, "0", 2) == 0) {
        value->val.ival = 0;
    } else {
        return 0;
    }

    return 1;
}

Status set_session_var(Session *sess, char *var_name, char *val)
{
    return set_config_var(sess->config, var_name, val);
}

Status set_buffer_var(Buffer *buffer, char *var_name, char *val)
{
    return set_config_var(buffer->config, var_name, val);
}

static Status set_config_var(HashMap *config, char *var_name, char *val)
{
    if (config == NULL || var_name == NULL || val == NULL) {
        return get_error(ERR_INVALID_VAR, "Invalid entry");
    }

    if (!is_valid_var(var_name)) {
        return get_error(ERR_INVALID_VAR, "Invalid variable name %s", var_name);
    }

    ConfigVariableDescriptor *var = hashmap_get(config, var_name);

    if (var == NULL) {
        var = clone_config_var_descriptor(var_name);

        if (var == NULL) {
            return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to set config value");
        }

        hashmap_set(config, var_name, var);
    }

    Value value;
    
    if (!conversion_functions[var->default_value.type](val, &value)) {
        return get_error(ERR_INVALID_VAL, "Invalid value \"%s\" for variable %s", value, var_name);
    }

    if (var->custom_validator != NULL) {
        if (!var->custom_validator(value)) {
            /* TODO It would be useful to know why the value isn't valid */
            return get_error(ERR_INVALID_VAL, "Invalid value \"%s\" for variable %s", value, var_name);
        }
    }

    Value old_value = var->default_value;
    var->default_value = value; 

    if (var->on_change_event != NULL) {
        return var->on_change_event(curr_sess, old_value, value);
    }

    free_value(old_value);

    return STATUS_SUCCESS;
}

int config_bool(char *var_name)
{
    Buffer *buffer = curr_sess->active_buffer;

    ConfigVariableDescriptor *var = hashmap_get(buffer->config, var_name);

    if (var == NULL) {
        var = hashmap_get(curr_sess->config, var_name);
    }

    if (var == NULL || var->default_value.type != VAL_TYPE_BOOL) {
        /* TODO Add error to session error queue */
        return 0;
    }

    return var->default_value.val.ival;
}

