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
    struct {
        Buffer *cmd_buffer; /* Used for command input e.g. Find & Replace, file name input, etc... */
        char *cmd_text; /* Command instruction/description */
        int cancelled; /* Did the user quit the last prompt */
        List *history;
        size_t history_index;
    } cmd_prompt; 
    CommandType exclude_cmd_types; /* Types of commands that shouldn't run */
    size_t buffer_num;
    size_t active_buffer_index;
    size_t menu_first_buffer_index;
    size_t empty_buffer_num;
    int msgs_enabled;
    List *search_history;
    List *replace_history;
    List *command_history;
    HashMap *filetypes;
    HashMap *syn_defs;
    HashMap *themes;
} Session;

Session *se_new(void);
int se_init(Session *, char **, int);
void se_free(Session *);
int se_add_buffer(Session *, Buffer *);
int se_set_active_buffer(Session *, size_t);
Buffer *se_get_buffer(const Session *, size_t);
int se_remove_buffer(Session *, Buffer *);
Status se_make_cmd_buffer_active(Session *, const char *, List *, int);
Status se_update_cmd_prompt_text(Session *, const char *);
int se_end_cmd_buffer_active(Session *);
int se_cmd_buffer_active(const Session *);
char *se_get_cmd_buffer_text(const Session *);
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
Status se_get_buffer_index(const Session *, const char *, int *);
Status se_add_search_to_history(Session *, char *);
Status se_add_replace_to_history(Session *, char *);
Status se_add_cmd_to_history(Session *, char *);
Status se_add_filetype_def(Session *, FileType *);
Status se_add_syn_def(Session *, SyntaxDefinition *, const char *);
int se_is_valid_syntaxtype(Session *, const char *);
const SyntaxDefinition *se_get_syntax_def(const Session *, const Buffer *);
int se_is_valid_theme(Session *, const char *);
Status se_add_theme(Session *, Theme *, const char *);

#endif
