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

#define CE_VAL(sessionref,bufferref) (ConfigEntity) { .sess = (sessionref), .buffer = (bufferref) }

#define CFG_TABWIDTH_MIN 1
#define CFG_TABWIDTH_MAX 8

typedef enum {
    CL_SESSION = 1,
    CL_BUFFER = 1 << 1
} ConfigLevel;

typedef enum {
    CT_SYNTAX,
    CT_THEME
} ConfigType;

typedef enum {
    CV_LINEWRAP,
    CV_LINENO,
    CV_TABWIDTH,
    CV_WEDRUNTIME,
    CV_FILETYPE,
    CV_SYNTAX,
    CV_SYNTAXTYPE,
    CV_THEME,
    CV_EXPANDTAB,
    CV_AUTOINDENT,
    CV_FILEFORMAT,
    CV_ENTRY_NUM
} ConfigVariable;

typedef struct {
    Session *sess;
    Buffer *buffer;
} ConfigEntity;

typedef struct {
    char *name;
    char *short_name;  
    ConfigLevel config_levels;
    Value default_value;
    Status (*custom_validator)(ConfigEntity, Value);
    Status (*on_change_event)(ConfigEntity, Value, Value);
} ConfigVariableDescriptor;

int cf_str_to_var(const char *, ConfigVariable *);
ConfigLevel cf_get_config_levels(ConfigVariable);
Status cf_init_session_config(Session *);
int cf_populate_config(const HashMap *, HashMap *, ConfigLevel);
void cf_load_config_def(Session *, ConfigType, const char *);
void cf_free_config(HashMap *);
Status cf_load_config(Session *, const char *);
Status cf_load_config_if_exists(Session *, const char *, const char *);
Status cf_set_named_var(ConfigEntity, ConfigLevel, char *, Value);
Status cf_set_var(ConfigEntity, ConfigLevel, ConfigVariable, Value);
Status cf_print_var(ConfigEntity, ConfigLevel, const char *);
int cf_bool(const HashMap *, ConfigVariable);
long cf_int(const HashMap *, ConfigVariable);
const char *cf_string(const HashMap *, ConfigVariable);

#endif
