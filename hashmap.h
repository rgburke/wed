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

/* Simple hashmap implementation using the murmurhash2 hash function.
 * Currently only accepts (char *) for keys and can grow but not shrink
 * when resizing. Enhancing it would be straightforward, however it's
 * sufficient for the moment. */

typedef struct {
    List *buckets; /* A resizable array of buckets */
    size_t entry_num; /* Items in hashmap */
    size_t bucket_num; /* Bucket list size */
} HashMap;

typedef struct HashMapNode HashMapNode;

/* Each bucket can contain a linked list of HashMapNodes */
struct HashMapNode {
    void *key; /* The key that was used for this entry */
    void *value; /* Values have to be pointers */
    uint32_t hash; /* Stored to avoid recalculation when resizing */
    HashMapNode *next; /* Next node in bucket or NULL */
};

HashMap *new_hashmap(void);
HashMap *new_sized_hashmap(size_t size);
int hashmap_set(HashMap *, const char *key, void *value);
void *hashmap_get(const HashMap *, const char *key);
int hashmap_delete(HashMap *, const char *key);
void hashmap_clear(HashMap *);
size_t hashmap_size(const HashMap *);
const char **hashmap_get_keys(const HashMap *);
void free_hashmap(HashMap *);
void free_hashmap_values(HashMap *, void (*free_func)(void *));

#endif

