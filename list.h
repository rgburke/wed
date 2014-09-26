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

#ifndef WED_LIST_H
#define WED_LIST_H

#include <stddef.h>
 
#define LIST_ALLOC 10
#define LIST_EXPAND 1
#define LIST_SHRINK -1

/* Simple list implementation */
/* TODO Consider inlining some of these functions */

typedef struct {
    void **values;
    size_t size;
    size_t allocated;
} List;

List *new_list();
List *new_sized_list(size_t);
size_t list_size(List *);
void *list_get(List *, size_t);
void list_set(List *, void *, size_t);
int list_add(List *, void *);
int list_add_at(List *, void *, size_t);
void *list_pop(List *);
void *list_remove_at(List *, size_t);
void list_clear(List *);
void free_list(List *);

#endif
