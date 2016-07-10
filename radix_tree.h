/*
 * Copyright (C) 2016 Richard Burke
 *
 * Based on article http://kukuruku.co/hub/algorithms/radix-trees
 * examining radix trees by Nikolai Ershov
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

#ifndef WED_RADIX_TREE_H
#define WED_RADIX_TREE_H

#include <stddef.h>

typedef struct RadixTreeNode RadixTreeNode;
typedef void (*FreeFunction)(void *);

/* Node structure used to construct the tree */
struct RadixTreeNode {
    char *key; /* String part contained by this node */
    size_t key_len; /* String length (includes null character for entry node) */
    void *data; /* Data stored at this node */
    RadixTreeNode *sibling; /* The next node at this level in the tree */
    RadixTreeNode *child; /* If NULL this node represents an entry stored in
                             the tree. Otherwise this node is just part of
                             a larger string entry */
};

/* Radix Tree wrapper structure */
typedef struct {
    RadixTreeNode *root; /* Root node of the tree, initially NULL */
    size_t entries; /* The number of string keys in the tree (not nodes) */
} RadixTree;

RadixTree *rt_new(void);
void rt_free(RadixTree *);
void rt_free_including_entries(RadixTree *, FreeFunction);
size_t rt_entries(const RadixTree *);
int rt_find(const RadixTree *, const char *str, size_t str_len,
            void **data, int *is_prefix);
int rt_insert(RadixTree *, const char *str, size_t str_len, void *data);
int rt_delete(RadixTree *, const char *str, size_t str_len);

#endif
