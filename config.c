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
#include <assert.h>
#include "config.h"
#include "value.h"
#include "hashmap.h"
#include "util.h"
#include "command.h"
#include "buffer.h"
#include "config_parse_util.h"
#include "config_parse.h"
#include "display.h"
#include "build_config.h"

#define CFG_FILE_NAME "wedrc"
#define CFG_SYSTEM_DIR "/etc"
#define CFG_FILETYPES_FILE_NAME "filetypes.wed"
#define CFG_USER_DIR "wed"

static Status cf_path_append(const char *path, const char *append,
                             char **result);
static void cf_free_cvd(ConfigVariableDescriptor *);
static const char *cf_get_config_type_string(ConfigType);
static Status cf_is_valid_var(ConfigEntity, ConfigLevel, ConfigVariable,
                              ConfigVariableDescriptor **var_ptr);
static const ConfigVariableDescriptor *cf_get_variable(const HashMap *config,
                                                       ConfigVariable);
static Status cf_tabwidth_validator(ConfigEntity, Value);
static Status cf_filetype_validator(ConfigEntity, Value);
static Status cf_filetype_on_change_event(ConfigEntity, Value, Value);
static Status cf_syntaxtype_validator(ConfigEntity, Value);
static Status cf_theme_validator(ConfigEntity, Value);
static Status cf_theme_on_change_event(ConfigEntity, Value, Value);
static Status cf_fileformat_validator(ConfigEntity, Value);
static Status cf_fileformat_on_change_event(ConfigEntity, Value, Value);

static const ConfigVariableDescriptor cf_default_config[CV_ENTRY_NUM] = {
    [CV_LINEWRAP]   = { "linewrap"  , "lw" , CL_SESSION | CL_BUFFER, BOOL_VAL_STRUCT(1)        , NULL                   , NULL },
    [CV_LINENO]     = { "lineno"    , "ln" , CL_SESSION | CL_BUFFER, BOOL_VAL_STRUCT(1)        , NULL                   , NULL },
    [CV_TABWIDTH]   = { "tabwidth"  , "tw" , CL_SESSION | CL_BUFFER, INT_VAL_STRUCT(8)         , cf_tabwidth_validator  , NULL },
    [CV_WEDRUNTIME] = { "wedruntime", "wrt", CL_SESSION            , STR_VAL_STRUCT(WEDRUNTIME), NULL                   , NULL },
    [CV_FILETYPE]   = { "filetype"  , "ft" , CL_BUFFER             , STR_VAL_STRUCT("")        , cf_filetype_validator  , cf_filetype_on_change_event },
    [CV_SYNTAX]     = { "syntax"    , "sy" , CL_SESSION            , BOOL_VAL_STRUCT(1)        , NULL                   , NULL },
    [CV_SYNTAXTYPE] = { "syntaxtype", "st" , CL_BUFFER             , STR_VAL_STRUCT("")        , cf_syntaxtype_validator, NULL },
    [CV_THEME]      = { "theme"     , "th" , CL_SESSION            , STR_VAL_STRUCT("default") , cf_theme_validator     , cf_theme_on_change_event },
    [CV_EXPANDTAB]  = { "expandtab" , "et" , CL_SESSION | CL_BUFFER, BOOL_VAL_STRUCT(0)        , NULL                   , NULL },
    [CV_AUTOINDENT] = { "autoindent", "ai" , CL_SESSION | CL_BUFFER, BOOL_VAL_STRUCT(1)        , NULL                   , NULL },
    [CV_FILEFORMAT] = { "fileformat", "ff" , CL_BUFFER             , STR_VAL_STRUCT("unix")    , cf_fileformat_validator, cf_fileformat_on_change_event }
};

static const size_t cf_var_num = ARRAY_SIZE(cf_default_config,
                                            ConfigVariableDescriptor);

int cf_str_to_var(const char *str, ConfigVariable *config_variable)
{
    assert(!is_null_or_empty(str));

    for (size_t k = 0; k < cf_var_num; k++) {
        if (strcmp(cf_default_config[k].name, str) == 0 ||
            strcmp(cf_default_config[k].short_name, str) == 0) {

            if (config_variable != NULL) {
                *config_variable = k;
            }

            return 1;
        }
    }

    return 0;
}

ConfigLevel cf_get_config_levels(ConfigVariable config_variable)
{
    assert(config_variable < CV_ENTRY_NUM);
    return cf_default_config[config_variable].config_levels;
}

/* This function runs on session creation */
Status cf_init_session_config(Session *sess)
{
    HashMap *config = sess->config;

    if (config == NULL) {
        /* We know the hashmap size so ensure we have a low load
         * factor and don't have to resize the hashmap */
        config = sess->config = new_sized_hashmap(cf_var_num * 2);
    }

    if (!cf_populate_config(NULL, config, CL_SESSION)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to load config");
    }
    
    se_add_error(sess, cf_load_config_if_exists(sess, CFG_SYSTEM_DIR,
                                                "/" CFG_FILE_NAME));

    const char *home_path = getenv("HOME"); 

    se_add_error(sess, cf_load_config_if_exists(sess, home_path,
                                                "/." CFG_FILE_NAME));

    const char *wed_run_time = cf_string(sess->config, CV_WEDRUNTIME);

    /* Load filetypes as they are used to drive syntax selection */
    se_add_error(sess, cf_load_config_if_exists(sess, wed_run_time,
                                                "/" CFG_FILETYPES_FILE_NAME));

    char *wed_user_dir = NULL;

    Status status = cf_path_append(home_path, "/." CFG_USER_DIR, &wed_user_dir);

    if (STATUS_IS_SUCCESS(status)) {
        if (access(wed_user_dir, F_OK) != -1) {
            /* Load user filetype overrides */
            se_add_error(sess,
                         cf_load_config_if_exists(sess, wed_user_dir, 
                                                  "/" CFG_FILETYPES_FILE_NAME));
        }

        free(wed_user_dir);
    } else {
        se_add_error(sess, status);
    }

    return STATUS_SUCCESS;
}

void cf_free_config(HashMap *config)
{
    if (config == NULL) {
        return;
    }

    for (size_t k = 0; k < cf_var_num; k++) {
        ConfigVariableDescriptor *cvd = hashmap_get(config,
                                                    cf_default_config[k].name);
        cf_free_cvd(cvd);
    }

    free_hashmap(config);
}

static void cf_free_cvd(ConfigVariableDescriptor *cvd)
{
    if (cvd == NULL) {
        return;
    }

    va_free_value(cvd->default_value);
    free(cvd);
}

/* Initialise config from existing config
 * i.e. populate buffer config from session config */
int cf_populate_config(const HashMap *src_config, HashMap *dst_config, 
                       ConfigLevel config_level)
{
    ConfigVariableDescriptor *clone;
    const ConfigVariableDescriptor *orig;

    for (size_t k = 0; k < cf_var_num; k++) {
        if (!(cf_default_config[k].config_levels & config_level)) {
            continue;
        }

        clone = malloc(sizeof(ConfigVariableDescriptor));

        if (clone == NULL) {
            return 0;
        }

        if (src_config == NULL) {
            orig = &cf_default_config[k];
        } else {
            orig = hashmap_get(src_config, cf_default_config[k].name);

            if (orig == NULL) {
                orig = &cf_default_config[k];
            }
        }

        memcpy(clone, orig, sizeof(ConfigVariableDescriptor));

        if (!STATUS_IS_SUCCESS(va_deep_copy_value(clone->default_value,
                                                  &clone->default_value))) {
            return 0;
        }

        if (!(hashmap_set(dst_config, clone->name, clone) && 
              hashmap_set(dst_config, clone->short_name, clone))) {
            return 0;
        }
    }

    return 1;
}

static const char *cf_get_config_type_string(ConfigType config_type)
{
    static const char *config_types[] = {
        [CT_SYNTAX] = "syntax",
        [CT_THEME]  = "theme"
    };

    assert(config_type < ARRAY_SIZE(config_types, const char *));

    return config_types[config_type];
}

/* Config block definitions can be loaded by name.
 * The convention is best explained with an example:
 * Enter <C-\>st=c; then wed will load WEDRUNTIME/syntax/c.wed
 * if it exists followed by ~/.wed/syntax/c.wed if it exists */
void cf_load_config_def(Session *sess, ConfigType cf_type,
                        const char *config_name)
{
    assert(!is_null_or_empty(config_name));

    if (is_null_or_empty(config_name)) {
        return;
    }

    const char *config_type = cf_get_config_type_string(cf_type);

    const char *file_name_fmt = "/%s/%s.wed";
    size_t file_path_length = strlen(file_name_fmt) - 4 + strlen(config_type)
                              + strlen(config_name) + 1;

    char *file_name = malloc(file_path_length);

    if (file_name == NULL) {
        se_add_error(sess, st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - ",
                                        "Unable to load config file"));
        return;
    }

    snprintf(file_name, file_path_length, file_name_fmt, 
             config_type, config_name);

    const char *wed_run_time = cf_string(sess->config, CV_WEDRUNTIME);

    se_add_error(sess, cf_load_config_if_exists(sess, wed_run_time, file_name));

    const char *home_path = getenv("HOME"); 
    char *wed_user_dir = NULL;

    Status status = cf_path_append(home_path, "/." CFG_USER_DIR, &wed_user_dir);

    if (STATUS_IS_SUCCESS(status)) {
        se_add_error(sess, cf_load_config_if_exists(sess, wed_user_dir,
                                                    file_name));
        free(wed_user_dir);
    } else {
        se_add_error(sess, status);
    }

    free(file_name);
}

Status cf_load_config_if_exists(Session *sess, const char *dir,
                                const char *file)
{
    if (is_null_or_empty(dir) || is_null_or_empty(file)) {
        return STATUS_SUCCESS;
    }

    char *config_path = NULL;          

    RETURN_IF_FAIL(cf_path_append(dir, file, &config_path));

    Status status = STATUS_SUCCESS;

    if (access(config_path, F_OK) != -1) {
        status = cf_load_config(sess, config_path);
    }

    free(config_path);

    return status;
}

static Status cf_path_append(const char *path, const char *append,
                             char **result)
{
    assert(path != NULL);
    assert(append != NULL);
    assert(result != NULL);

    char *res = concat(path, append);

    if (res == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - "
                            "Unable to construct config path"); 
    }

    *result = res;

    return STATUS_SUCCESS;
}

Status cf_load_config(Session *sess, const char *config_file_path)
{
    return cp_parse_config_file(sess, CL_SESSION, config_file_path);
}

Status cf_set_named_var(ConfigEntity entity, ConfigLevel config_level, 
                        char *var_name, Value value)
{
    ConfigVariable config_variable;

    if (is_null_or_empty(var_name) ||
        !cf_str_to_var(var_name, &config_variable)) {
        return st_get_error(ERR_INVALID_VAR, "Invalid config variable \"%s\"",
                            var_name);
    }

    return cf_set_var(entity, config_level, config_variable, value);
}

Status cf_set_var(ConfigEntity entity, ConfigLevel config_level, 
                  ConfigVariable config_variable, Value value)
{
    ConfigVariableDescriptor *var;

    RETURN_IF_FAIL(cf_is_valid_var(entity, config_level,
                                   config_variable, &var));

    if (var->default_value.type != value.type) {
        /* Allow Boolean variables to be set with integer values */
        if (var->default_value.type == VAL_TYPE_BOOL &&
            value.type == VAL_TYPE_INT) {
            value = BOOL_VAL(IVAL(value) ? 1 : 0);
        } else {
            return st_get_error(ERR_INVALID_VAL,
                                "%s must have value of type %s",
                                var->name,
                                va_get_value_type(var->default_value));
        }
    }

    if (var->custom_validator != NULL) {
        RETURN_IF_FAIL(var->custom_validator(entity, value));
    }

    Value old_value = var->default_value;
    RETURN_IF_FAIL(va_deep_copy_value(value, &var->default_value));

    Status status = STATUS_SUCCESS;

    if (var->on_change_event != NULL) {
        status = var->on_change_event(entity, old_value, value);
    }

    va_free_value(old_value);

    char config_msg[MAX_MSG_SIZE];
    char *value_str = va_to_string(value);
    snprintf(config_msg, MAX_MSG_SIZE, "Set %s=%s", var->name, value_str);
    free(value_str);
    se_add_msg(entity.sess, config_msg);

    return status;
}

static Status cf_is_valid_var(ConfigEntity entity, ConfigLevel config_level, 
                              ConfigVariable config_variable, 
                              ConfigVariableDescriptor **var_ptr)
{
    assert(config_variable < CV_ENTRY_NUM);

    HashMap *config = NULL;

    if (config_level & CL_SESSION) {
        config = entity.sess->config;
    } else if (config_level & CL_BUFFER) {
        config = entity.buffer->config;
    }

    assert(config != NULL);

    const char *var_name = cf_default_config[config_variable].name;

    ConfigVariableDescriptor *var = hashmap_get(config, var_name);

    if (var == NULL) {
        if (!(cf_default_config[config_variable].config_levels &
              config_level)) {
            return st_get_error(ERR_INCORRECT_CONFIG_LEVEL, 
                                "Variable %s can only be referenced "
                                "at the %s level",
                                var_name,
                                (config_level & CL_BUFFER) ?
                                "session" : "buffer");
        }

        return st_get_error(ERR_INVALID_VAR,
                            "Invalid config variable %s", var_name);
    }

    *var_ptr = var;

    return STATUS_SUCCESS;
}

static const ConfigVariableDescriptor *cf_get_variable(const HashMap *config,
                                                       ConfigVariable config_var
                                                      )
{
    assert(config_var >= 0 && config_var < CV_ENTRY_NUM);

    const char *var_name = cf_default_config[config_var].name;
    const ConfigVariableDescriptor *var = hashmap_get(config, var_name);

    assert(var != NULL);

    return var;
}

Status cf_print_var(ConfigEntity entity, ConfigLevel config_level,
                    const char *var_name)
{
    ConfigVariable config_variable;

    if (is_null_or_empty(var_name) ||
        !cf_str_to_var(var_name, &config_variable)) {
        return st_get_error(ERR_INVALID_VAR, "Invalid config variable \"%s\"",
                            var_name);
    }

    ConfigVariableDescriptor *var;

    RETURN_IF_FAIL(cf_is_valid_var(entity, config_level,
                                   config_variable, &var));

    char var_msg[MAX_MSG_SIZE];
    char *value_str = va_to_string(var->default_value);
    ValueType value_type = var->default_value.type;
    const char *fmt = (value_type == VAL_TYPE_STR ? "%s=\"%s\"" : "%s=%s");
    snprintf(var_msg, MAX_MSG_SIZE, fmt, var->name, value_str);
    free(value_str);
    se_add_msg(entity.sess, var_msg);

    return STATUS_SUCCESS;
}

int cf_bool(const HashMap *config, ConfigVariable config_var)
{
    const ConfigVariableDescriptor *var = cf_get_variable(config, config_var);

    assert(var->default_value.type == VAL_TYPE_BOOL);

    return BVAL(var->default_value);
}

long cf_int(const HashMap *config, ConfigVariable config_var)
{
    const ConfigVariableDescriptor *var = cf_get_variable(config, config_var);

    assert(var->default_value.type == VAL_TYPE_INT);

    return IVAL(var->default_value);
}

const char *cf_string(const HashMap *config, ConfigVariable config_var)
{
    const ConfigVariableDescriptor *var = cf_get_variable(config, config_var);

    assert(var->default_value.type == VAL_TYPE_STR);

    return SVAL(var->default_value);
}

static Status cf_tabwidth_validator(ConfigEntity entity, Value value)
{
    (void)entity;

    if (IVAL(value) < CFG_TABWIDTH_MIN || IVAL(value) > CFG_TABWIDTH_MAX) {
        return st_get_error(ERR_INVALID_TABWIDTH,
                            "tabwidth value must be in range %d - %d inclusive",
                            CFG_TABWIDTH_MIN, CFG_TABWIDTH_MAX);
    }

    return STATUS_SUCCESS;
}

static Status cf_filetype_validator(ConfigEntity entity, Value value)
{
    if (SVAL(value) != NULL && *SVAL(value) == '\0') {
        /* Allow filetype to be set to none */
        return STATUS_SUCCESS;
    }

    FileType *file_type = hashmap_get(entity.sess->filetypes, SVAL(value));

    if (file_type == NULL) {
        return st_get_error(ERR_INVALID_FILETYPE,
                            "No filetype with name \"%s\" exists",
                            SVAL(value));
    }

    return STATUS_SUCCESS;
}

static Status cf_filetype_on_change_event(ConfigEntity entity, Value old_val,
                                          Value new_val)
{
    (void)old_val;
    (void)new_val;

    if (!se_initialised(entity.sess)) {
        return STATUS_SUCCESS;
    }

    /* filetype drives syntaxtype */
    se_determine_syntaxtype(entity.sess, entity.buffer);

    return STATUS_SUCCESS;
}

static Status cf_syntaxtype_validator(ConfigEntity entity, Value value)
{
    if (SVAL(value) != NULL && *SVAL(value) == '\0') {
        /* Allow syntaxtype to be set to none */
        return STATUS_SUCCESS;
    }

    if (!se_is_valid_syntaxtype(entity.sess, SVAL(value))) {
        return st_get_error(ERR_INVALID_SYNTAXTYPE,
                            "No syntaxtype with name \"%s\" exists",
                            SVAL(value));
    }

    return STATUS_SUCCESS;
}

static Status cf_theme_validator(ConfigEntity entity, Value value)
{
    if (!se_is_valid_theme(entity.sess, SVAL(value))) {
        return st_get_error(ERR_INVALID_THEME,
                            "No theme with name \"%s\" exists",
                            SVAL(value));
    }

    return STATUS_SUCCESS;
}

static Status cf_theme_on_change_event(ConfigEntity entity, Value old_val,
                                       Value new_val)
{
    (void)old_val;
    (void)new_val;

    const Theme *theme = se_get_active_theme(entity.sess);
    init_color_pairs(theme);    

    return STATUS_SUCCESS;
}

static Status cf_fileformat_validator(ConfigEntity entity, Value value)
{
    (void)entity;
    FileFormat file_format;

    if (!bf_get_fileformat(SVAL(value), &file_format)) {
        return st_get_error(ERR_INVALID_FILE_FORMAT,
                            "Invalid file format \"%s\"",
                            SVAL(value));
    }

    return STATUS_SUCCESS;
}

static Status cf_fileformat_on_change_event(ConfigEntity entity, Value old_val,
                                            Value new_val)
{
    (void)old_val;

    FileFormat file_format;
    Buffer *buffer = entity.buffer;

    bf_get_fileformat(SVAL(new_val), &file_format);
    bf_set_fileformat(buffer, file_format);

    return STATUS_SUCCESS;
}

