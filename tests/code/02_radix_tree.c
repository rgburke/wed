#include <stdlib.h>
#include <string.h>
#include "tap.h"
#include "../../radix_tree.h"

static void radix_tree_insert(RadixTree *rtree, const char *strings[],
                              size_t string_num);
static void radix_tree_find(RadixTree *rtree, const char *strings[],
                            size_t string_num);
static void radix_tree_delete(RadixTree *rtree, const char *strings[],
                              size_t string_num);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    plan(93);

    RadixTree *rtree = rt_new();

    if (!ok(rtree != NULL, "Create RadixTree")) {
        return exit_status();
    }

    const char *strings[] = {
        "ab", "abc", "abdc", "abde", "abcd", "bb", "abb", "aba", "abbc",
        "bbd", "baba", "abca", "abcb", "abd", "a", "aa", "add", "acd"
    };

    const size_t string_num = sizeof(strings) / sizeof(const char *);

    radix_tree_insert(rtree, strings, string_num);
    radix_tree_find(rtree, strings, string_num);
    radix_tree_delete(rtree, strings, string_num);

    rt_free(rtree);

    return exit_status();
}

static void radix_tree_insert(RadixTree *rtree, const char *strings[],
                              size_t string_num)
{
    msg("Insert:");

    for (size_t k = 0; k < string_num; k++) {
        ok(rt_insert(rtree, strings[k], strlen(strings[k]), NULL),
           "Insert string");
    }

    ok(rt_entries(rtree) == string_num, "Entry count correct after insertions");
}

static void radix_tree_find(RadixTree *rtree, const char *strings[],
                            size_t string_num)
{
    msg("Find:");

    for (size_t k = 0; k < string_num; k++) {
        ok(rt_find(rtree, strings[k], strlen(strings[k]), NULL, NULL),
           "Found inserted string");
    }

    int is_prefix;

    ok(!rt_find(rtree, "b", 1, NULL, &is_prefix), "No false positive match");
    ok(is_prefix, "Identified as prefix");
    ok(!rt_find(rtree, "adc", 3, NULL, &is_prefix), "No false positive match");
    ok(!is_prefix, "Not identified as prefix");
    ok(!rt_find(rtree, "bbb", 3, NULL, &is_prefix), "No false positive match");
    ok(!is_prefix, "Not identified as prefix");
    ok(!rt_find(rtree, "ad", 2, NULL, &is_prefix), "No false positive match");
    ok(is_prefix, "Identified as prefix");
    ok(!rt_find(rtree, "ac", 2, NULL, &is_prefix), "No false positive match");
    ok(is_prefix, "Identified as prefix");
    ok(rt_find(rtree, "abc", 3, NULL, &is_prefix), "Found inserted string");
    ok(!is_prefix, "Entry not identified as prefix");
}

static void radix_tree_delete(RadixTree *rtree, const char *strings[],
                              size_t string_num)
{
    msg("Delete:");
    size_t deletions = string_num / 2;

    for (size_t k = 0; k < string_num; k++) {
        if (k & 1) {
            ok(rt_delete(rtree, strings[k], strlen(strings[k])),
               "Deleted string");
        }
    }

    ok(rt_entries(rtree) == string_num - deletions,
       "Entry count correct after deletions");

    for (size_t k = 0; k < string_num; k++) {
        if (k & 1) {
            ok(!rt_find(rtree, strings[k], strlen(strings[k]), NULL, NULL),
               "Deleted string not found");
        } else {
            ok(rt_find(rtree, strings[k], strlen(strings[k]), NULL, NULL),
               "Non-Deleted string found");
        }
    }

    ok(!rt_delete(rtree, "b", 1), "No false positive delete");
    ok(!rt_delete(rtree, "adc", 3), "No false positive delete");
    ok(!rt_delete(rtree, "bbb", 3), "No false positive delete");
    ok(!rt_delete(rtree, "abdd", 4), "No false positive delete");

    for (size_t k = 0; k < string_num; k++) {
        if (!(k & 1)) {
            ok(rt_delete(rtree, strings[k], strlen(strings[k])),
               "Deleted string");
        }
    }

    ok(rt_entries(rtree) == 0, "Entry count correct after all entries deleted");
    ok(rtree->root == NULL, "Root node is NULL after all entries deleted");
}

