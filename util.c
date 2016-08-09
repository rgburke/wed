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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"

void warn(const char *error_msg)
{
    fprintf(stderr, "%s\n", error_msg);
}

void fatal(const char *error_msg)
{
    warn(error_msg);
    warn("Fatal error encountered");
    exit(1);
}

int roundup_div(size_t dividend, size_t divisor)
{
    if (divisor == 0) {
        return 0;
    }

    return (dividend + (divisor - 1)) / divisor;
}

char *concat(const char *str1, const char *str2)
{
    return concat_all(2, str1, str2);
}

char *concat_all(size_t str_num, ...)
{
    if (str_num == 0) {
        return NULL;
    }

    const char *strings[str_num];
    size_t strings_len[str_num];
    size_t result_str_len = 1;

    va_list varg_list;
    const char *str;

    va_start(varg_list, str_num);

    for (size_t k = 0; k < str_num; k++) {
        str = va_arg(varg_list, const char *); 

        if (str == NULL) {
            str = "NULL";
        }

        strings_len[k] = strlen(str);
        result_str_len += strings_len[k];
        strings[k] = str;
    }

    char *result_str = malloc(result_str_len);

    if (result_str == NULL) {
        goto cleanup;
    }

    char *iter = result_str;

    for (size_t k = 0; k < str_num; k++) {
        if (strings_len[k] != 0) {
            memcpy(iter, strings[k], strings_len[k]);
            iter += strings_len[k];
        }
    }

    *iter = '\0';

cleanup:
    va_end(varg_list);

    return result_str;
}

int is_null_or_empty(const char *str)
{
    return str == NULL || *str == '\0';
}

size_t occurrences(const char *str, const char *sub_str)
{
    if (is_null_or_empty(str) || is_null_or_empty(sub_str)) {
        return 0;
    }

    size_t occurrences = 0;
    size_t sub_str_len = strlen(sub_str);
    const char *iter = str;

    while ((iter = strstr(iter, sub_str)) != NULL) {
        occurrences++;
        iter += sub_str_len;
    }

    return occurrences;
}

/* TODO Simple but inefficient */
char *replace(const char *str, const char *to_replace, const char *replacement)
{
    if (str == NULL || is_null_or_empty(to_replace) || replacement == NULL) {
        return NULL;
    }

    size_t occurs = occurrences(str, to_replace);

    if (occurs == 0) {
        return strdup(str);
    }

    size_t to_rep_len = strlen(to_replace);
    size_t rep_len = strlen(replacement);
    size_t new_str_len = strlen(str);

    if (rep_len > to_rep_len) {
        new_str_len += occurs * (rep_len - to_rep_len);
    } else if (to_rep_len > rep_len) {
        new_str_len -= occurs * (to_rep_len - rep_len);
    }

    char *new_str = malloc(new_str_len + 1);

    if (new_str == NULL) {
        return NULL;
    }

    const char *last = str, *iter = str;
    char *new = new_str;
    size_t byte_num;

    while ((iter = strstr(iter, to_replace)) != NULL) {
        byte_num = iter - last;

        if (byte_num > 0) {
            memcpy(new, last, byte_num);
            new += byte_num;
        }

        memcpy(new, replacement, rep_len);
        new += rep_len;
        iter += to_rep_len;
        last = iter;
    }

    while ((*new++ = *last++)) ;

    return new_str;
}

void *memrchr(const void *str, int val, size_t bytes)
{
    const unsigned char *iter = ((const unsigned char *)str) + (bytes - 1);
    const unsigned char c = (unsigned char)val;
    
    while (bytes--) {
        if (*iter == c) {
            return (void *)iter;
        }

        iter--;
    }

    return NULL;
}

void bytes_to_str(size_t bytes, char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return;
    }

    static const char *units[] = {
        "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
    };

    size_t unit_index = 0;
    long double size = bytes;

    while (size > 1000) {
        size /= 1024;
        unit_index++;
    }

    assert(unit_index < ARRAY_SIZE(units,  const char *));

    int decimal_places = unit_index == 0 ? 0 : 2;

    snprintf(buf, buf_len, "%.*Lf %s", decimal_places, size, units[unit_index]);
}

