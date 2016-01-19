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
 
/* Default list allocation size */
#ifndef LIST_ALLOC
#define LIST_ALLOC 10
#endif

/* Can provide custom function to free list entries */
typedef void (*ListEntryFree)(void *);
/* Used for sorting */
typedef int (*ListComparator)(const void *, const void *);

/* Simple list implementation */
typedef struct {
    void **values; /* Array of (void *). Values have to be pointers */
    size_t size; /* Number of items in list */
    size_t allocated; /* Currently allocated memory */
} List;

List *list_new(void);
List *list_new_prealloc(size_t size);
List *list_new_sized(size_t size);
size_t list_size(const List *);
void *list_get(const List *, size_t index);
void *list_get_first(const List *);
void *list_get_last(const List *);
int list_set(List *, void *value, size_t index);
int list_add(List *, void *value);
int list_add_at(List *, void *value, size_t index);
void *list_pop(List *);
void *list_remove_at(List *, size_t index);
void list_sort(List *, ListComparator);
void list_nullify(List *);
void list_clear(List *);
void list_free_values(List *);
void list_free_values_custom(List *, ListEntryFree);
void list_free_all(List *);
void list_free_all_custom(List *, ListEntryFree);
void list_free(List *);

#endif
