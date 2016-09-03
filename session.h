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

#include "wed.h"
#include "shared.h"
#include "buffer.h"
#include "status.h"
#include "hashmap.h"
#include "file_type.h"
#include "syntax_manager.h"
#include "theme.h"
#include "prompt.h"
#include "input.h"
#include "clipboard.h"
#include "command.h"
#include "ui.h"

#if WED_FEATURE_LUA
#include "wed_lua.h"
#endif

#define MAX_KEY_STR_SIZE 100

/* Top level structure containing all state.
 * A new session is created when wed is invoked. */
struct Session {
    Buffer *buffers; /* Linked list of buffers */
    Buffer *active_buffer; /* The buffer currently being edited */
    Buffer *error_buffer; /* Buffer which stores error messages */
    Buffer *msg_buffer; /* Buffer which stores messages */
    KeyMap key_map; /* Maps keyboard inputs to commands */
    Clipboard clipboard; /* Handles copy and paste to system clipboard */
    HashMap *config; /* Stores config variables */
    Prompt *prompt; /* Used to control prompt */
    CommandType exclude_cmd_types; /* Types of commands that shouldn't run */
    size_t buffer_num; /* Number of buffers being edited */
    size_t active_buffer_index; /* Index of active buffer */
    size_t menu_first_buffer_index; /* Keep track of first buffer displayed
                                       in the menu */
    size_t empty_buffer_num; /* Number of anonymous buffers created
                                i.e. [new 1], [new 2] */
    int msgs_enabled; /* Toggle whether messages sent to session are stored */
    List *search_history; /* Previous searches */
    List *replace_history; /* Previous replace text entries */
    List *command_history; /* Previous commands run */
    List *lineno_history; /* Previous line numbers entered */
    List *buffer_history; /* Previous buffer names entered */
    HashMap *filetypes; /* Store filetypes by name */
    HashMap *themes; /* Store themes by name */
    SyntaxManager sm; /* Manage syntax definitions */
    int initialised; /* True if session finished initialising */
    int finished; /* True if the session has finished */
    List *cfg_buffer_stack; /* Stack of YY_BUFFER_STATE buffers (used for
                               parsing config files) */
    char prev_key[MAX_KEY_STR_SIZE]; /* Previous keypress */
    WedOpt wed_opt; /* Command line option values */
    UI *ui; /* UI interface */
    InputBuffer input_buffer; /* Input is buffered in this structure */
#if WED_FEATURE_LUA
    LuaState *ls;
#endif
};

typedef struct Session Session;

Session *se_new(void);
int se_init(Session *, const WedOpt *, char *buffer_paths[], int buffer_num);
void se_free(Session *);
int se_add_buffer(Session *, Buffer *);
int se_is_valid_buffer_index(const Session *, size_t buffer_index);
int se_get_buffer_index(const Session *, const Buffer *,
                        size_t *buffer_index_ptr);
int se_set_active_buffer(Session *, size_t buffer_index);
Buffer *se_get_buffer(const Session *, size_t buffer_index);
int se_remove_buffer(Session *, Buffer *);
Status se_make_prompt_active(Session *, PromptType, const char *prompt_text,
                             List *history, int show_last_cmd);
int se_end_prompt(Session *);
int se_prompt_active(const Session *);
void se_exclude_command_type(Session *, CommandType);
void se_enable_command_type(Session *, CommandType);
int se_command_type_excluded(const Session *, CommandType);
int se_add_error(Session *, Status);
int se_has_errors(const Session *);
void se_clear_errors(Session *);
int se_add_msg(Session *, const char *msg);
int se_has_msgs(const Session *);
void se_clear_msgs(Session *);
int se_msgs_enabled(const Session *);
int se_enable_msgs(Session *);
int se_disable_msgs(Session *);
Status se_add_new_buffer(Session *, const char *file_path, int is_stdin);
Status se_add_new_empty_buffer(Session *);
Status se_get_buffer_index_by_path(const Session *, const char *file_path,
                                   int *buffer_index_ptr);
Status se_add_search_to_history(Session *, const char *search_text);
Status se_add_replace_to_history(Session *, const char *replace_text);
Status se_add_cmd_to_history(Session *, const char *cmd_text);
Status se_add_lineno_to_history(Session *, const char *lineno_text);
Status se_add_buffer_to_history(Session *, const char *buffer_text);
Status se_add_filetype_def(Session *, FileType *);
void se_determine_syntaxtype(Session *, Buffer *);
int se_is_valid_syntaxtype(Session *, const char *syn_type);
const SyntaxDefinition *se_get_syntax_def(const Session *, const Buffer *);
int se_is_valid_theme(Session *, const char *theme);
Status se_add_theme(Session *, Theme *, const char *theme_name);
const Theme *se_get_active_theme(const Session *);
int se_initialised(const Session *);
void se_save_key(Session *, const char *key);
const char *se_get_prev_key(const Session *);
int se_session_finished(const Session *);
void se_set_session_finished(Session *);
const char *se_get_file_type_display_name(const Session *, const Buffer *);
void se_determine_filetypes_if_unset(Session *, Buffer *);

#endif
