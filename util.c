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
#include "util.h"

void fatal(const char *error_msg)
{
    fprintf(stderr, "%s\n", error_msg);
    exit(1);
}

void *alloc(size_t size)
{
    void *ptr = malloc(size); 

    if (ptr == NULL) {
        fatal("Failed to allocate memory"); 
    }

    return ptr;
}

void *ralloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);

    if (ptr == NULL) {
        fatal("Failed to allocate memory"); 
    }

    return ptr;
}

int roundup_div(int dividend, int divisor)
{
    if (divisor == 0) {
        return 0;
    }

    return (dividend + (divisor - 1)) / divisor;
}

int sign(int k) {
    return (k > 0) - (k < 0);
}

size_t utf8_char_num(const char *str)
{
    size_t char_num = 0;

    while (*str) {
        if ((*str & 0xc0) != 0x80) {
            char_num++;
        }

        str++;
    }

    return char_num;
}

char *strdupe(const char *str)
{
    if (str == NULL) {
        return NULL;
    }

    char *copy = alloc(strlen(str) + 1);

    if (copy == NULL) {
        return NULL;
    }

    strcpy(copy, str);

    return copy;
}
