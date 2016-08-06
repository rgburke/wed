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

#ifndef WED_LUA_H
#define WED_LUA_H

#include <lua.h>
#include "status.h"
#include "syntax.h"
#include "hashmap.h"

struct Session;

/* Structure through which wed interacts with lua */
typedef struct {
    lua_State *state; /* Maintains Lua state. Used to interface with Lua */
    struct Session *sess; /* Session reference */
    HashMap *token_map; /* Map Scintillua tokens to wed tokens */
} LuaState;

LuaState *ls_new(struct Session *);
void ls_free(LuaState *);
Status ls_init(LuaState *);
Status ls_load_syntax_def(LuaState *, const char *);
SyntaxMatches *ls_generate_matches(LuaState *, const char *syntax_type,
                                   const char *str, size_t str_len);

#endif
