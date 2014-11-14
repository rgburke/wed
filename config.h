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

#include "variable.h"
#include "status.h"
#include "hashmap.h"
#include "session.h"

typedef struct {
    char *name;
    char *short_name;  
    Value default_value;
    int (*custom_validator)(Value);
    Status (*on_change_event)(Session *, Value, Value);
} ConfigVariableDescriptor;

Status init_config(Session *);
void free_config(HashMap *);
Status load_config(Session *, char *);
Status set_session_var(Session *, char *, char *);
int config_bool(Session *, char *);

#endif
