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

#ifndef WED_VARIABLE_H
#define WED_VARIABLE_H

#include "status.h"

typedef enum {
    VAL_TYPE_BOOL,
    VAL_TYPE_INT,
    VAL_TYPE_FLOAT,
    VAL_TYPE_STR
} ValueType;

/* Used to pass multiple types to a command through a single instance variable */
typedef struct {
    ValueType type;
    union {
        long ival;
        double fval;
        char *sval;
    } val;
} Value;

#define BOOL_VAL_STRUCT(bvalue) { .type = VAL_TYPE_BOOL, .val = { .ival = (bvalue) } }
#define INT_VAL_STRUCT(ivalue)  { .type = VAL_TYPE_INT , .val = { .ival = (ivalue) } }
#define STR_VAL_STRUCT(svalue)  { .type = VAL_TYPE_STR , .val = { .sval = (svalue) } }
#define INT_VAL(ivalue)  (Value) INT_VAL_STRUCT(ivalue)
#define STR_VAL(svalue)  (Value) STR_VAL_STRUCT(svalue)
#define BOOL_VAL(bvalue) (Value) BOOL_VAL_STRUCT(bvalue)

const char *get_value_type(Value);
Status deep_copy_value(Value, Value *);
void free_value(Value);

#endif

