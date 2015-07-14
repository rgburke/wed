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
#include <assert.h>
#include "value.h"
#include "util.h"
#include "status.h"

#define VALUE_STRING_CONVERT_SIZE 100

static const char *value_types[] = {
    "Boolean",
    "Integer",
    "Float",
    "String",
    "Regex"
};

const char *va_get_value_type(Value value)
{
    return va_value_type_string(value.type);
}

const char *va_value_type_string(ValueType value_type)
{
    assert(value_type < (sizeof(value_types) / sizeof(char *)));

    return value_types[value_type];
}

Status va_deep_copy_value(Value value, Value *new_val)
{
    if (!STR_BASED_VAL(value)) {
        *new_val = value;
        return STATUS_SUCCESS;
    }

    const char *curr_val = va_str_val(value);

    if (curr_val == NULL) {
        *new_val = value;
        return STATUS_SUCCESS;
    }

    char *str_val = strdupe(curr_val);

    if (str_val == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy value");
    }

    if (value.type == VAL_TYPE_STR) {
        *new_val = STR_VAL(str_val);
    } else if (value.type == VAL_TYPE_REGEX) {
        *new_val = REGEX_VAL(str_val, RVAL(value).modifiers);
    }

    return STATUS_SUCCESS;
}

char *va_to_string(Value value)
{
    switch (value.type) {
        case VAL_TYPE_STR:
            return strdupe(SVAL(value));
        case VAL_TYPE_BOOL:
            return strdupe(IVAL(value) ? "true" : "false");
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
        case VAL_TYPE_REGEX:
            return strdupe(RVAL(value).regex_pattern);
        default:
            assert(!"Invalid value type");
            break;
    }

    return NULL;
}

const char *va_str_val(Value value)
{
    if (value.type == VAL_TYPE_STR) {
        return SVAL(value);
    } else if (value.type == VAL_TYPE_REGEX) {
        return RVAL(value).regex_pattern;
    }

    assert(!"Invalid value type");

    return NULL;
}

void va_free_value(Value value)
{
    if (!STR_BASED_VAL(value)) {
        return;
    }

    if (value.type == VAL_TYPE_STR) {
        free(SVAL(value));
    } else if (value.type == VAL_TYPE_REGEX) {
        free(RVAL(value).regex_pattern);
    }
}
