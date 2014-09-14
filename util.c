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
#include "util.h"

List *new_list()
{
    return new_sized_list(LIST_ALLOC);        
}

List *new_sized_list(size_t size)
{
    List *list = alloc(sizeof(List));

    list->size = 0;
    list->allocated = size;
    list->values = alloc(sizeof(void **) * list->allocated);

    return list; 
}

static void resize_list(List *list, int resize_type)
{
    int new_size = list->allocated + (((list->allocated / 4) + LIST_ALLOC) * resize_type);

    if (new_size > 0) {
        list->allocated = new_size;
        list->values = realloc(list->values, sizeof(void **) * list->allocated);
    }    
}

size_t list_size(List *list)
{
    return list->size;
}

void *list_get(List *list, size_t index)
{
    void *value = NULL;

    if (index < list->size) {
        value = list->values[index];
    }

    return value;
}

void list_set(List *list, void *value, size_t index)
{
    if (index < list->size) {
        list->values[index] = value;
    }
}

void list_add(List *list, void *value)
{
    if (list->size == list->allocated) {
        resize_list(list, LIST_EXPAND);
    }
    
    list->values[list->size++] = value;    
}

void list_add_at(List *list, void *value, size_t index)
{
    if (index >= list->size) {
        return;
    }

    if (list->size == list->allocated) {
        resize_list(list, LIST_EXPAND);
    }

    for (size_t k = list->size++; k > index; k--) {
        list->values[k] = list->values[k - 1];
    }

    list->values[index] = value;
}

void *list_pop(List *list)
{
    void *value = NULL;

    if (list->size) {
        value = list->values[--(list->size)];

        if (list->size > LIST_ALLOC && list->size < (list->allocated / 2)) {
            resize_list(list, LIST_SHRINK);
        }
    }

    return value;    
}

void *list_remove_at(List *list, size_t index)
{
    void *value = NULL;

    if (list->size && index < list->size) {
        value = list->values[index];

        for (size_t k = index; k < list->size; k++) {
            list->values[k] = list->values[k + 1];   
        }

        if (--(list->size) > LIST_ALLOC && list->size < (list->allocated / 2)) {
            resize_list(list, LIST_SHRINK);
        }
    }

    return value;
}

void free_list(List *list)
{
    if (list) {
        free(list->values);
        free(list);
    } 
}

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

size_t utf8_char_num(char *str)
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

