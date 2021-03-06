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

/* Value types supported by the Value struct */
typedef enum {
    VAL_TYPE_BOOL          = 1 << 0,
    VAL_TYPE_INT           = 1 << 1,
    VAL_TYPE_FLOAT         = 1 << 2,
    VAL_TYPE_STR           = 1 << 3,
    VAL_TYPE_REGEX         = 1 << 4,
    VAL_TYPE_SHELL_COMMAND = 1 << 5
} ValueType;

/* Regex in string form */
typedef struct {
    char *regex_pattern; /* Regex pattern */
    int modifiers; /* PCRE modifiers */
} Regex;

/* Value struct to abstract dealing with different types */
typedef struct {
    ValueType type; /* The type of value stored */
    /* The actual value is stored in the val union */
    union {
        long ival;
        double fval;
        char *sval;
        Regex rval;
    } val;
} Value;

/* Helper macros to create and access value instances */

#define BOOL_VAL_STRUCT(bvalue) \
        { .type = VAL_TYPE_BOOL , .val = { .ival = (bvalue) } }
#define INT_VAL_STRUCT(ivalue) \
        { .type = VAL_TYPE_INT  , .val = { .ival = (ivalue) } }
#define STR_VAL_STRUCT(svalue) \
        { .type = VAL_TYPE_STR  , .val = { .sval = (svalue) } }
#define REGEX_VAL_STRUCT(rvalue,rmod) \
{ .type = VAL_TYPE_REGEX, \
  .val = { .rval = { .regex_pattern = (rvalue), .modifiers = (rmod) } } }
#define CMD_VAL_STRUCT(cmdvalue) \
        { .type = VAL_TYPE_SHELL_COMMAND, .val = { .sval = (cmdvalue) } }

#define BOOL_VAL(bvalue)       (Value) BOOL_VAL_STRUCT(bvalue)
#define INT_VAL(ivalue)        (Value) INT_VAL_STRUCT(ivalue)
#define STR_VAL(svalue)        (Value) STR_VAL_STRUCT(svalue)
#define REGEX_VAL(rvalue,rmod) (Value) REGEX_VAL_STRUCT(rvalue,rmod)
#define CMD_VAL(cmdvalue)      (Value) CMD_VAL_STRUCT(cmdvalue)

#define BVAL(value) IVAL(value)
#define IVAL(value) (value).val.ival
#define FVAL(value) (value).val.fval
#define SVAL(value) (value).val.sval
#define RVAL(value) (value).val.rval
#define CVAL(value) (value).val.sval

#define STR_BASED_VAL(value) \
    ((value).type == VAL_TYPE_STR || \
     (value).type == VAL_TYPE_REGEX || \
     (value).type == VAL_TYPE_SHELL_COMMAND)

const char *va_get_value_type(Value);
const char *va_value_type_string(ValueType);
const char *va_multi_value_type_string(ValueType);
Status va_deep_copy_value(Value value, Value *new_val);
char *va_to_string(Value);
const char *va_str_val(Value);
void va_free_value(Value);

#endif

