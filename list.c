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
#include "list.h"
#include "util.h"

static int grow_required(List *);
static int shrink_required(List *);
static void resize_list(List *, int);

List *new_list()
{
    return new_sized_list(LIST_ALLOC);        
}

List *new_sized_list(size_t size)
{
    List *list = alloc(sizeof(List));

    list->size = 0;
    list->allocated = size;
    list->values = alloc(sizeof(void *) * list->allocated);

    return list; 
}

static int grow_required(List *list)
{
    return list->size == list->allocated;
}

static int shrink_required(List *list)
{
    return list->size < (list->allocated / 2);
}

static void resize_list(List *list, int resize_type)
{
    size_t new_size = list->allocated + ((list->allocated / 2) * resize_type);
    list->allocated = new_size;
    list->values = realloc(list->values, sizeof(void *) * list->allocated);
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
    if (grow_required(list)) {
        resize_list(list, LIST_EXPAND);
    }

    list->values[list->size++] = value;    
}

void list_add_at(List *list, void *value, size_t index)
{
    if (index >= list->size) {
        return;
    }

    if (grow_required(list)) {
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

    if (list->size > 0) {
        value = list->values[--list->size];

        if (shrink_required(list)) {
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

        list->size--;

        if (shrink_required(list)) {
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
