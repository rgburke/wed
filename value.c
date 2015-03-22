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

#include <stdlib.h>
#include <stdio.h>
#include "value.h"
#include "util.h"
#include "status.h"

#define VALUE_STRING_CONVERT_SIZE 100

static const char *value_types[] = {
    "Boolean",
    "Integer",
    "Float",
    "String"
};

const char *get_value_type(Value value)
{
    return value_types[value.type];
}

Status deep_copy_value(Value value, Value *new_val)
{
    *new_val = value;

    if (value.type != VAL_TYPE_STR || value.val.sval == NULL) {
        return STATUS_SUCCESS;
    }

    new_val->val.sval = strdupe(value.val.sval);

    if (new_val->val.sval == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy value");
    }

    return STATUS_SUCCESS;
}

char *to_string(Value value)
{
    switch (value.type) {
        case VAL_TYPE_STR:
            return strdupe(value.val.sval);
        case VAL_TYPE_BOOL:
            return strdupe(value.val.ival ? "true" : "false");
        case VAL_TYPE_INT:
        case VAL_TYPE_FLOAT:
            {
                char *num_str = malloc(VALUE_STRING_CONVERT_SIZE);

                if (num_str == NULL) {
                    return NULL;
                }

                const char *format = (value.type == VAL_TYPE_INT ? "%d" : "%f");

                snprintf(num_str, VALUE_STRING_CONVERT_SIZE, format, value.val);

                return num_str;
            }
        default:
            break;
    }

    return NULL;
}

void free_value(Value value)
{
    if (value.type != VAL_TYPE_STR || value.val.sval == NULL) {
        return;
    }

    free(value.val.sval);
}
