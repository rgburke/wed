/*
 * Copyright (C) 2015 Richard Burke
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

#ifndef WED_SEARCH_H
#define WED_SEARCH_H

#include "shared.h"
#include "status.h"
#include "buffer_pos.h"
#include "text_search.h"
#include "regex_search.h"
#include "search_options.h"

typedef enum {
    BST_TEXT,
    BST_REGEX
} BufferSearchType;

struct BufferSearch {
    SearchOptions opt;
    BufferPos last_match_pos;
    BufferSearchType search_type;
    BufferSearchType last_search_type;
    union {
        TextSearch text;
        RegexSearch regex;
    } type;
};

typedef struct BufferSearch BufferSearch;

Status bs_init(BufferSearch *, const char *, size_t);
Status bs_reinit(BufferSearch *, const char *, size_t);
Status bs_init_default_opt(BufferSearch *);
void bs_free(BufferSearch *);
Status bs_find_next(BufferSearch *, const BufferPos *, int *);

#endif
