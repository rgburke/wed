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

#ifndef WED_SESSION_H
#define WED_SESSION_H

#include "shared.h"
#include "buffer.h"
#include "status.h"
#include "hashmap.h"
#include "file_type.h"
#include "syntax.h"
#include "theme.h"
#include "prompt.h"

#define MAX_KEY_STR_SIZE 50

/* Top level structure containing all state.
 * A new session is created when wed is invoked. */
typedef struct {
    Buffer *buffers; /* Linked list buffers */
    Buffer *active_buffer; /* The buffer currently being edited */
    Buffer *error_buffer;
    Buffer *msg_buffer;
    HashMap *keymap; /* Maps keyboard inputs to commands */
    TextSelection clipboard; /* Stores copied and cut text */
    HashMap *config; /* Stores config variables */
    Prompt *prompt;
    CommandType exclude_cmd_types; /* Types of commands that shouldn't run */
    size_t buffer_num;
    size_t active_buffer_index;
    size_t menu_first_buffer_index;
    size_t empty_buffer_num;
    int msgs_enabled;
    List *search_history;
    List *replace_history;
    List *command_history;
    List *lineno_history;
    List *buffer_history;
    HashMap *filetypes;
    HashMap *syn_defs;
    HashMap *themes;
    int initialised;
    List *cfg_buffer_stack;
    char prev_key[MAX_KEY_STR_SIZE]; /* TODO create Input structure and put this in there */
} Session;

Session *se_new(void);
int se_init(Session *, char **, int);
void se_free(Session *);
int se_add_buffer(Session *, Buffer *);
int se_is_valid_buffer_index(const Session *, size_t);
int se_get_buffer_index(const Session *, const Buffer *, size_t *);
int se_set_active_buffer(Session *, size_t);
Buffer *se_get_buffer(const Session *, size_t);
int se_remove_buffer(Session *, Buffer *);
Status se_make_prompt_active(Session *, PromptType, const char *, List *, int);
int se_end_prompt(Session *);
int se_prompt_active(const Session *);
void se_set_clipboard(Session *, TextSelection);
void se_exclude_command_type(Session *, CommandType);
void se_enable_command_type(Session *, CommandType);
int se_command_type_excluded(const Session *, CommandType);
int se_add_error(Session *, Status);
int se_has_errors(const Session *);
void se_clear_errors(Session *);
int se_add_msg(Session *, const char *);
int se_has_msgs(const Session *);
void se_clear_msgs(Session *);
int se_msgs_enabled(const Session *);
int se_enable_msgs(Session *);
int se_disable_msgs(Session *);
Status se_add_new_buffer(Session *, const char *);
Status se_add_new_empty_buffer(Session *);
Status se_get_buffer_index_by_path(const Session *, const char *, int *);
Status se_add_search_to_history(Session *, char *);
Status se_add_replace_to_history(Session *, char *);
Status se_add_cmd_to_history(Session *, char *);
Status se_add_lineno_to_history(Session *, char *);
Status se_add_buffer_to_history(Session *, char *);
Status se_add_filetype_def(Session *, FileType *);
Status se_add_syn_def(Session *, SyntaxDefinition *, const char *);
void se_determine_syntaxtype(Session *, Buffer *);
int se_is_valid_syntaxtype(Session *, const char *);
const SyntaxDefinition *se_get_syntax_def(const Session *, const Buffer *);
int se_is_valid_theme(Session *, const char *);
Status se_add_theme(Session *, Theme *, const char *);
const Theme *se_get_active_theme(const Session *);
int se_initialised(const Session *);
void se_save_key(Session *, const char *);
const char *se_get_prev_key(const Session *);

#endif
