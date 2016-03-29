/*
 * Copyright (C) 2016 Richard Burke
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

#ifndef WED_SOURCE_HIGHLIGHT_INTERFACE_H
#define WED_SOURCE_HIGHLIGHT_INTERFACE_H

#include "status.h"

/* The C interface to the C++ source-highlight functionality */

struct SyntaxMatches;

typedef struct {
    void *tokenizer; /* Instance of a wed::Tokenizer class */
} SourceHighlight;

Status sh_init(SourceHighlight *sh, const char *lang_dir, const char *lang);
struct SyntaxMatches *sh_tokenize(const SourceHighlight *sh, const char *str);
void sh_free(SourceHighlight *sh);

#endif
