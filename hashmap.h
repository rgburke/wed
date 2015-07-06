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

#ifndef WED_HASHMAP_H
#define WED_HASHMAP_H

#include <stdint.h>
#include "list.h"

typedef struct HashMapNode HashMapNode;

/* Simple hashmap implementation using the murmurhash2 hash function.
 * Currently only accepts char * for keys and can grow but not shrink when resizing.
 * Enhancing it would be straightforward, however it's sufficient for the moment. */

typedef struct {
    List *buckets;
    size_t entry_num;
    size_t bucket_num;
} HashMap;

/* Each bucket can contain a linked list of HashMapNodes */
struct HashMapNode {
    void *key;
    void *value;
    uint32_t hash; /* Stored so we don't have to recalculate it when resizing */
    HashMapNode *next;
};

HashMap *new_hashmap(void);
HashMap *new_sized_hashmap(size_t);
int hashmap_set(HashMap *, const char *, void *);
void *hashmap_get(HashMap *, const char *);
int hashmap_delete(HashMap *, const char *);
void hashmap_clear(HashMap *);
size_t hashmap_size(HashMap *);
const char **hashmap_get_keys(HashMap *);
void free_hashmap(HashMap *);
void free_hashmap_values(HashMap *, void (*)(void *));

#endif


