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

#ifndef WED_CONFIG_H
#define WED_CONFIG_H

#include "value.h"
#include "status.h"
#include "hashmap.h"
#include "session.h"
#include "buffer.h"

#define CE_VAL(sessionref,bufferref) \
            (ConfigEntity) { \
                .sess = (sessionref), \
                .buffer = (bufferref) \
            }

#define CFG_TABWIDTH_MIN 1
#define CFG_TABWIDTH_MAX 8

#define CFG_FILE_EXPLORER_WIDTH_DEFAULT 30
#define CFG_FILE_EXPLORER_WIDTH_MIN 5
#define CFG_FILE_EXPLORER_WIDTH_MAX 1024
#define CFG_FILE_EXPLORER_POSITION_LEFT "left"
#define CFG_FILE_EXPLORER_POSITION_RIGHT "right"
#define CFG_FILE_EXPLORER_POSITION_IS_LEFT(config) \
    (strcmp(cf_string((config), CV_FILE_EXPLORER_POSITION), \
            CFG_FILE_EXPLORER_POSITION_LEFT) == 0)

#define CFG_SYNTAX_HORIZON_DEFAULT 20
#define CFG_SYNTAX_HORIZON_MIN 0

/* Some variables apply at the session and buffer levels
 * e.g. ln=0; in ~/.wedrc turns off line numbers for all buffers.
 * However when in wed typing <C-\>ln=0; only affects the active buffer.
 * The ConfigLevel enum is used to specify at which level we should 
 * interpret commands. e.g. CL_SESSION for ~/.wedrc and CL_BUFFER for
 * the command prompt. */
typedef enum {
    CL_SESSION = 1,
    CL_BUFFER = 1 << 1
} ConfigLevel;

/* Syntax and theme definitions are stored using a similar hierarchy
 * and can be loaded in a common way. A syntax or theme def can
 * be loaded by the same function invoked with a name and a ConfigType */
typedef enum {
    CT_SYNTAX,
    CT_THEME
} ConfigType;

/* These are variables which modify behaviour in wed and can be set by
 * the user through a config file or the command prompt */
typedef enum {
    CV_LINEWRAP,
    CV_LINENO,
    CV_TABWIDTH,
    CV_EXPANDTAB,
    CV_AUTOINDENT,
    CV_COLORCOLUMN,
    CV_BUFFEREND,
    CV_WEDRUNTIME,
    CV_SYNTAX,
    CV_SYNTAX_HORIZON,
    CV_THEME,
    CV_SYNTAXDEFTYPE,
    CV_SHDATADIR,
    CV_MOUSE,
    CV_FILE_EXPLORER,
    CV_FILE_EXPLORER_WIDTH,
    CV_FILE_EXPLORER_POSITION,
    CV_FILETYPE,
    CV_SYNTAXTYPE,
    CV_FILEFORMAT,
    CV_ENTRY_NUM
} ConfigVariable;

/* Container struct */
typedef struct {
    Session *sess; /* Session ref */
    Buffer *buffer; /* Affected buffer, can be NULL if setting
                       Session level variable */
} ConfigEntity;

/* This struct defines a variable in wed */
typedef struct {
    char *name; /* Variable name */
    char *short_name; /* Short variable name for convenience */ 
    ConfigLevel config_levels; /* Bit mask of the config levels this
                                  variable can apply at */
    Value default_value; /* Default value */
    /* Validator function which fires when a variable is set to
     * a new value. If this function returns a failure status then
     * the variable's value is not changed */
    Status (*custom_validator)(ConfigEntity, Value);
    /* When a variable's value has changed this function is fired. This
     * function fires after the variable has been set so the return status
     * of this function doesn't have any effect on the variable */
    Status (*on_change_event)(ConfigEntity, Value old_val, Value new_val);
    const char *description; /* Describes what the variable represents */
} ConfigVariableDescriptor;

int cf_str_to_var(const char *str, ConfigVariable *);
ConfigLevel cf_get_config_levels(ConfigVariable);
Status cf_init_session_config(Session *);
int cf_populate_config(const HashMap *src_config, HashMap *dst_config,
                       ConfigLevel);
void cf_load_config_def(Session *, ConfigType, const char *config_name);
void cf_free_config(HashMap *config);
Status cf_load_config(Session *, const char *config_file_path);
Status cf_load_config_if_exists(Session *, const char *dir, const char *file);
Status cf_set_named_var(ConfigEntity, ConfigLevel, char *var_name, Value);
Status cf_set_var(ConfigEntity, ConfigLevel, ConfigVariable, Value);
Status cf_print_var(ConfigEntity, ConfigLevel, const char *var_name);
int cf_bool(const HashMap *config, ConfigVariable);
long cf_int(const HashMap *config, ConfigVariable);
const char *cf_string(const HashMap *config, ConfigVariable);
Status cf_generate_variable_table(HelpTable *);
void cf_free_variable_table(HelpTable *);

#endif
