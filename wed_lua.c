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

#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "wed_lua.h"
#include "session.h"
#include "config.h"
#include "util.h"

/* Files and directories under WEDRUNTIME related to lua */
#define WED_LUA_DIR "lua"
#define WED_LUA_INIT_FILE "wed_init.lua"
#define WED_LUA_LEXERS_DIR "lexers"
/* Lua variables and functions created and initalised by WED_LUA_INIT_FILE */
#define WED_LUA_GLOBAL_VAR "wed"
#define WED_LUA_LOAD_LEXER_FUNC "load_lexer"
#define WED_LUA_TOKENIZE_FUNC "tokenize"

static int ls_add_to_package_path(LuaState *, const char *path);

/* Structure mapping a Scintillua token to a wed token */
typedef struct {
    const char *sl_token; /* Scintillua token name */
    SyntaxToken wed_token; /* wed syntax token */
} ScintilluaTokenMap;

static ScintilluaTokenMap ls_token_map[] = {
    { "bracebad"    , ST_ERROR      },
    { "bracelight"  , ST_NORMAL     },
    { "calltip"     , ST_NORMAL     },
    { "class"       , ST_STATEMENT  },
    { "comment"     , ST_COMMENT    },
    { "constant"    , ST_CONSTANT   },
    { "controlchar" , ST_NORMAL     },
    { "default"     , ST_NORMAL     },
    { "embedded"    , ST_NORMAL     },
    { "error"       , ST_ERROR      },
    { "function"    , ST_IDENTIFIER },
    { "identifier"  , ST_NORMAL     },
    { "indentguide" , ST_NORMAL     },
    { "keyword"     , ST_STATEMENT  },
    { "label"       , ST_STATEMENT  },
    { "lexerpath"   , ST_NORMAL     },
    { "linenumber"  , ST_NORMAL     },
    { "nothing"     , ST_NORMAL     },
    { "number"      , ST_CONSTANT   },
    { "operator"    , ST_NORMAL     },
    { "preprocessor", ST_SPECIAL    },
    { "regex"       , ST_CONSTANT   },
    { "string"      , ST_CONSTANT   },
    { "type"        , ST_TYPE       },
    { "variable"    , ST_NORMAL     }
};

LuaState *ls_new(Session *sess)
{
    LuaState *ls = malloc(sizeof(LuaState));
    RETURN_IF_NULL(ls);

    ls->state = luaL_newstate();

    if (ls->state == NULL) {
        ls_free(ls);
        return NULL;
    }

    luaL_openlibs(ls->state);

    ls->sess = sess;

    ls->token_map = new_hashmap();

    if (ls->token_map == NULL) {
        ls_free(ls);
        return NULL;
    }

    const size_t token_num = ARRAY_SIZE(ls_token_map, ScintilluaTokenMap);
    char token_style_name[100];

    for (size_t k = 0; k < token_num; k++) {
        if (!hashmap_set(ls->token_map, ls_token_map[k].sl_token,
                         &ls_token_map[k].wed_token)) {
            ls_free(ls);
            return NULL;
        }

        /* Custom tokens are mapped to token styles which wed can map to
         * standard Scintillua token names. Below we add the token style names
         * to the map */
        snprintf(token_style_name, sizeof(token_style_name), "$(style.%s)",
                 ls_token_map[k].sl_token);

        if (!hashmap_set(ls->token_map, token_style_name,
                         &ls_token_map[k].wed_token)) {
            ls_free(ls);
            return NULL;
        }
    }

    return ls;
}

void ls_free(LuaState *ls)
{
    if (ls == NULL) {
        return;
    }

    if (ls->state != NULL) {
        lua_close(ls->state);
    }

    free_hashmap(ls->token_map);
    free(ls);
}

Status ls_init(LuaState *ls)
{
    Session *sess = ls->sess;
    const char *wrt = cf_string(sess->config, CV_WEDRUNTIME);
    Status status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                 "Unable to allocate memory for file path");

    char *wed_lua_dir_path = NULL;
    char *wed_lua_lexers_path = NULL;
    char *wed_lua_init_script = NULL;

    wed_lua_dir_path = concat(wrt, "/" WED_LUA_DIR);

    if (wed_lua_dir_path == NULL) {
        goto cleanup;
    }

    wed_lua_lexers_path = concat_all(4, wed_lua_dir_path, "/",
                                     WED_LUA_LEXERS_DIR, "/?.lua");

    if (wed_lua_lexers_path == NULL) {
        goto cleanup;
    }

    if (!ls_add_to_package_path(ls, wed_lua_lexers_path)) {
        goto cleanup;
    }

    wed_lua_init_script = concat_all(3, wed_lua_dir_path, "/",
                                     WED_LUA_INIT_FILE);

    if (wed_lua_init_script == NULL) {
        goto cleanup;
    }

    if (luaL_dofile(ls->state, wed_lua_init_script)) {
        status = st_get_error(ERR_LUA_ERROR, "Error occured when running wed "
                              "lua init script %s",
                              lua_tostring(ls->state, -1));
        lua_pop(ls->state, 1);
        goto cleanup;
    }

    status = STATUS_SUCCESS;

cleanup:
    free(wed_lua_dir_path);
    free(wed_lua_lexers_path);
    free(wed_lua_init_script);

    return status;
} 

static int ls_add_to_package_path(LuaState *ls, const char *path)
{
    lua_getglobal(ls->state, "package");
    lua_getfield(ls->state, -1, "path");
    const char *package_path = lua_tostring(ls->state, -1);
    
    char *package_path_new = concat_all(3, package_path, ";", path);

    if (package_path_new == NULL) {
        lua_pop(ls->state, 2);
        return 0;
    }
 
    lua_pop(ls->state, 1);
    lua_pushstring(ls->state, package_path_new);
    lua_setfield(ls->state, -2, "path");
    lua_pop(ls->state, 1);

    free(package_path_new);

    return 1;
}

Status ls_load_syntax_def(LuaState *ls, const char *syntax_type)
{
    lua_getglobal(ls->state, WED_LUA_GLOBAL_VAR);

    if (!lua_istable(ls->state, -1)) {
        lua_pop(ls->state, 1);
        return st_get_error(ERR_LUA_ERROR, "Unable to load variable %s",
                            WED_LUA_GLOBAL_VAR);
    }

    lua_getfield(ls->state, -1, WED_LUA_LOAD_LEXER_FUNC);

    if (!lua_isfunction(ls->state, -1)) {
        lua_pop(ls->state, 2);
        return st_get_error(ERR_LUA_ERROR, "Unable to load function %s.%s",
                            WED_LUA_GLOBAL_VAR, WED_LUA_LOAD_LEXER_FUNC);
    }

    lua_insert(ls->state, -2);
    lua_pushstring(ls->state, syntax_type);
    Status status = STATUS_SUCCESS;

    if (lua_pcall(ls->state, 2, 0, 0) != 0) {
        status = st_get_error(ERR_LUA_ERROR, "Loading lexer %s failed: %s",
                              syntax_type, lua_tostring(ls->state, -1));
        lua_pop(ls->state, 1);
    }

    return status;
}

SyntaxMatches *ls_generate_matches(LuaState *ls, const char *syntax_type,
                                   const char *str, size_t str_len)
{
    lua_getglobal(ls->state, WED_LUA_GLOBAL_VAR);

    if (!lua_istable(ls->state, -1)) {
        lua_pop(ls->state, 1);
        return NULL;
    }

    lua_getfield(ls->state, -1, WED_LUA_TOKENIZE_FUNC);

    if (!lua_isfunction(ls->state, -1)) {
        lua_pop(ls->state, 2);
        return NULL;
    }

    lua_insert(ls->state, -2);

    lua_pushstring(ls->state, syntax_type);
    lua_pushlstring(ls->state, str, str_len);
    
    if (lua_pcall(ls->state, 3, 1, 0) != 0) {
        lua_pop(ls->state, 1);
        return NULL;
    }

    if (!lua_istable(ls->state, -1)) {
        lua_pop(ls->state, 1);
        return NULL;
    }

    SyntaxMatches *syn_matches = sy_new_matches(0);

    if (syn_matches == NULL) {
        lua_pop(ls->state, 1);
        return NULL;
    }

    size_t token_array_size =
#if LUA_VERSION_NUM > 501
        lua_rawlen(ls->state, -1);
#else
        lua_objlen(ls->state, -1);
#endif
    const char *sl_token;
    lua_Number end_pos;
    SyntaxToken *wed_token;
    SyntaxMatch syn_match;
    size_t token_length;
    size_t offset = 0;
    
    for (size_t k = 1; k + 1 <= token_array_size; k += 2) {
        lua_rawgeti(ls->state, -1, k);

        if (!lua_isstring(ls->state, -1)) {
            lua_pop(ls->state, 1);
            continue;
        }

        sl_token = lua_tostring(ls->state, -1);

        lua_rawgeti(ls->state, -2, k + 1);

        if (!lua_isnumber(ls->state, -1)) {
            lua_pop(ls->state, 2);
            continue;
        }

        end_pos = lua_tonumber(ls->state, -1);
        lua_pop(ls->state, 2);

        token_length = (end_pos - 1) - offset;

        if (sl_token != NULL && strstr(sl_token, "whitespace") == NULL) {
            wed_token = hashmap_get(ls->token_map, sl_token);

            syn_match.token = wed_token == NULL ? ST_NORMAL : *wed_token;
            syn_match.offset = offset;
            syn_match.length = token_length;

            sy_add_match(syn_matches, &syn_match);
        }

        offset += token_length;
    }

    lua_pop(ls->state, 1);

    return syn_matches;
}

