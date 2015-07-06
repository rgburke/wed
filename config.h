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

typedef enum {
    CL_SESSION = 1,
    CL_BUFFER = 1 << 1
} ConfigLevel;

typedef struct {
    char *name;
    char *short_name;  
    ConfigLevel config_levels;
    Value default_value;
    Status (*custom_validator)(Session *, Value);
    Status (*on_change_event)(Session *, Value, Value);
} ConfigVariableDescriptor;

void cf_set_config_session(Session *);
Status cf_init_config(void);
void cf_end_config(void);
Status cf_init_session_config(Session *);
int cf_populate_default_config(HashMap *, ConfigLevel, int);
void cf_free_config(HashMap *);
Status cf_load_config(Session *, const char *);
Status cf_load_config_if_exists(Session *, const char *, const char *);
Status cf_set_var(Session *, ConfigLevel, char *, Value);
Status cf_set_session_var(Session *, char *, Value);
Status cf_set_buffer_var(Buffer *, char *, Value);
Status cf_print_var(Session *sess, const char *);
int cf_bool(const char *);
long cf_int(const char *);
const char *cf_string(const char *);
int cf_bf_bool(const char *, const Buffer *);
long cf_bf_int(const char *, const Buffer *);
const char *cf_bf_string(const char *, const Buffer *);

#endif
