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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "radix_tree.h"

static void rt_free_tree(RadixTreeNode *, FreeFunction);
static RadixTreeNode *rt_new_node(const char *str, size_t str_len, void *data,
                                  RadixTreeNode *sibling, RadixTreeNode *child);
static void rt_free_node(RadixTreeNode *node);
static size_t rt_prefix(const char *str, size_t str_len,
                        const char *key, size_t key_len);
static int rt_split(RadixTreeNode *node, size_t prefix_len);
static int rt_join(RadixTreeNode *parent);

RadixTree *rt_new(void)
{
    RadixTree *rtree = malloc(sizeof(RadixTree));

    if (rtree == NULL) {
        return rtree;
    }

    memset(rtree, 0, sizeof(RadixTree));
    return rtree;
}

void rt_free(RadixTree *rtree)
{
    if (rtree != NULL) {
        rt_free_tree(rtree->root, NULL);
        free(rtree);
    }
}

void rt_free_including_entries(RadixTree *rtree, FreeFunction free_func)
{
    if (rtree != NULL) {
        rt_free_tree(rtree->root, free_func);
        free(rtree);
    }
}

static void rt_free_tree(RadixTreeNode *node, FreeFunction free_func)
{
    if (node == NULL) {
        return;
    }

    if (free_func != NULL) {
        free_func(node->data);
    }

    rt_free_tree(node->child, free_func);
    rt_free_tree(node->sibling, free_func);
    rt_free_node(node);
}

static RadixTreeNode *rt_new_node(const char *str, size_t str_len, void *data,
                                  RadixTreeNode *sibling, RadixTreeNode *child)
{
    RadixTreeNode *node = malloc(sizeof(RadixTreeNode));

    if (node == NULL) {
        return node;
    }

    node->key = malloc(str_len);

    if (node->key == NULL) {
        free(node);
        return NULL;
    }

    memcpy(node->key, str, str_len);
    node->key_len = str_len;
    node->data = data;
    node->sibling = sibling;
    node->child = child;

    return node;
}

static void rt_free_node(RadixTreeNode *node)
{
    if (node == NULL) {
        return;
    }

    free(node->key);
    free(node);
}

size_t rt_entries(const RadixTree *rtree)
{
    return rtree->entries;
}

/* Determine the number of characters that match at the start of both strings
 * i.e. the prefix length */
static size_t rt_prefix(const char *str, size_t str_len,
                        const char *key, size_t key_len)
{
    size_t limit = str_len < key_len ? str_len : key_len;
    size_t idx = 0;

    while (idx < limit && str[idx] == key[idx]) {
        idx++;
    }

    return idx;
}

/* Search for a string, return true if in the tree and false if not. If the
 * string is not in the tree but is a prefix of an entry then set is_prefix
 * to true */
int rt_find(const RadixTree *rtree, const char *str,
            size_t str_len, void **data, int *is_prefix)
{
    if (is_prefix != NULL) {
        *is_prefix = 0;
    }

    if (str == NULL) {
        return 0;
    }

    assert(str[str_len] == '\0');
    str_len += 1;

    const RadixTreeNode *node = rtree->root;

    while (node != NULL) {
        size_t prefix_len = rt_prefix(str, str_len, node->key, node->key_len);

        if (prefix_len == 0) {
            node = node->sibling;
        } else if (prefix_len == str_len) {
            break;
        } else if (prefix_len == node->key_len) {
            str = str + prefix_len;
            str_len -= prefix_len;
            node = node->child;
        } else {
            if (prefix_len == (str_len - 1) && is_prefix != NULL) {
                *is_prefix = 1;
            }

            return 0;
        }
    }

    if (node == NULL) {
        if (*str == '\0' && is_prefix != NULL) {
            *is_prefix = 1;
        }

        return 0;
    } else if (data != NULL) {
        *data = node->data;
    }

    return 1;
}

/* Split a node into two as a new node now shares the first prefix_len
 * characters with this node */
static int rt_split(RadixTreeNode *node, size_t prefix_len)
{
    RadixTreeNode *split_node = rt_new_node(node->key + prefix_len,
                                            node->key_len - prefix_len,
                                            node->data, NULL, node->child);

    if (split_node == NULL) {
        return 0;
    }

    char *key = malloc(prefix_len);

    if (key == NULL) {
        free(split_node);
        return 0;
    }

    memcpy(key, node->key, prefix_len);
    free(node->key);

    node->key = key;
    node->key_len = prefix_len;
    node->child = split_node;
    node->data = NULL;

    return 1;
}

int rt_insert(RadixTree *rtree, const char *str, size_t str_len, void *data)
{
    if (str == NULL) {
        return 0;
    }

    assert(str[str_len] == '\0');
    str_len += 1;

    RadixTreeNode *node = rtree->root;
    RadixTreeNode *prev = NULL;
    size_t prefix_len = 0;

    while (node != NULL) {
        prefix_len = rt_prefix(str, str_len, node->key, node->key_len);

        if (prefix_len == 0) {
            prev = node;
            node = node->sibling;
        } else if (prefix_len < str_len) {
            if (prefix_len < node->key_len) {
                if (!rt_split(node, prefix_len)) {
                    return 0;
                }
            }

            prev = node;
            str = str + prefix_len;
            str_len -= prefix_len;
            node = node->child;
        } else {
            break;
        }
    }

    if (node == NULL) {
        node = rt_new_node(str, str_len, data, NULL, NULL);

        if (node == NULL) {
            return 0;
        }

        if (prev == NULL) {
            rtree->root = node;
        } else {
            if (prefix_len == 0) {
                prev->sibling = node;
            } else {
                prev->child = node;
            }
        }

        rtree->entries++;
    } else {
        /* Entry already exists so just update data */
        node->data = data;
        return 0;
    }

    return 1;
}

/* Merge a parent node with a child node that has no siblings together
 * i.e. keep the tree compressed */
static int rt_join(RadixTreeNode *parent)
{
    RadixTreeNode *child = parent->child;
    parent->key = realloc(parent->key, parent->key_len + child->key_len);

    if (parent->key == NULL) {
        return 0;
    }

    memcpy(parent->key + parent->key_len, child->key, child->key_len);
    parent->key_len += child->key_len;
    parent->data = child->data;
    parent->child = child->child;
    rt_free_node(child);

    return 1;
}

int rt_delete(RadixTree *rtree, const char *str, size_t str_len)
{
    if (str == NULL) {
        return 0;
    }

    assert(str[str_len] == '\0');
    str_len += 1;

    RadixTreeNode *node = rtree->root;
    RadixTreeNode *prev_sibling = NULL;
    RadixTreeNode *prev_parent = NULL;
    size_t prefix_len = 0;

    while (node != NULL) {
        prefix_len = rt_prefix(str, str_len, node->key, node->key_len);

        if (prefix_len == str_len) {
            break;
        } else if (prefix_len == 0) {
            prev_sibling = node;
            node = node->sibling;
        } else if (prefix_len == node->key_len) {
            str = str + prefix_len;
            str_len -= prefix_len;
            prev_parent = node;
            prev_sibling = NULL;
            node = node->child;
        } else {
            return 0;
        }
    }

    if (node != NULL) {
        if (prev_sibling != NULL) {
            prev_sibling->sibling = node->sibling;
        }
        if (prev_parent != NULL) {
            if (prev_parent->child == node) {
                prev_parent->child = node->sibling;
            }

            if (prev_parent->child != NULL &&
                prev_parent->child->sibling == NULL) {
                if (!rt_join(prev_parent)) {
                    return 0;
                }
            }
        }

        if (rtree->root == node) {
            rtree->root = node->sibling;
        }

        rt_free_node(node);
    } else {
        return 0;
    }

    rtree->entries--;

    return 1;
}

