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
    CL_SESSION,
    CL_BUFFER
} ConfigLevel;

typedef struct {
    char *name;
    char *short_name;  
    Value default_value;
    Status (*custom_validator)(Value);
    Status (*on_change_event)(Session *, Value, Value);
} ConfigVariableDescriptor;

void cf_set_config_session(Session *);
Status cf_init_config(void);
void cf_end_config(void);
Status cf_init_session_config(Session *);
void cf_free_config(HashMap *);
Status cf_load_config(Session *, char *);
Status cf_set_var(Session *, ConfigLevel, char *, Value);
Status cf_set_session_var(Session *, char *, Value);
Status cf_set_buffer_var(Buffer *, char *, Value);
Status cf_print_var(Session *sess, const char *);
int cf_bool(char *);
long cf_int(char *);

#endif
