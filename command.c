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

#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include "shared.h"
#include "status.h"
#include "command.h"
#include "session.h"
#include "display.h"
#include "buffer.h"
#include "variable.h"
#include "hashmap.h"
#include "util.h"
#include "input.h"

static void free_command(void *);

static Status bufferpos_change_line(Session *, Value, const char *, int *);
static Status bufferpos_change_char(Session *, Value, const char *, int *); 
static Status bufferpos_to_line_start(Session *, Value, const char *, int *);
static Status bufferpos_to_line_end(Session *, Value, const char *, int *);
static Status bufferpos_to_next_word(Session *, Value, const char *, int *);
static Status bufferpos_to_prev_word(Session *, Value, const char *, int *);
static Status bufferpos_to_buffer_start(Session *, Value, const char *, int *);
static Status bufferpos_to_buffer_end(Session *, Value, const char *, int *);
static Status bufferpos_change_page(Session *, Value, const char *, int *);
static Status buffer_insert_char(Session *, Value, const char *, int *);
static Status buffer_delete_char(Session *, Value, const char *, int *);
static Status buffer_backspace(Session *, Value, const char *, int *);
static Status buffer_insert_line(Session *, Value, const char *, int *);
static Status buffer_select_all_text(Session *, Value, const char *, int *);
static Status buffer_copy_selected_text(Session *, Value, const char *, int *);
static Status buffer_cut_selected_text(Session *, Value, const char *, int *);
static Status buffer_paste_text(Session *, Value, const char *, int *);
static Status buffer_save_file(Session *, Value, const char *, int *);
static Status session_change_tab(Session *, Value, const char *, int *);
static Status finished_processing_input(Session *, Value, const char *, int *);

static Status cmd_input_prompt(Session *, const char *);
static Status cancel_cmd_input_prompt(Session *, Value, const char *, int *);
static int update_command_function(Session *, const char *, CommandHandler);

static const Command commands[] = {
    { "<Up>"        , bufferpos_change_line    , INT_VAL_STRUCT(DIRECTION_UP)                           , CMDT_BUFFER_MOVE },
    { "<Down>"      , bufferpos_change_line    , INT_VAL_STRUCT(DIRECTION_DOWN)                         , CMDT_BUFFER_MOVE },
    { "<Right>"     , bufferpos_change_char    , INT_VAL_STRUCT(DIRECTION_RIGHT)                        , CMDT_BUFFER_MOVE },
    { "<Left>"      , bufferpos_change_char    , INT_VAL_STRUCT(DIRECTION_LEFT)                         , CMDT_BUFFER_MOVE },
    { "<Home>"      , bufferpos_to_line_start  , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<End>"       , bufferpos_to_line_end    , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<C-Right>"   , bufferpos_to_next_word   , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<C-Left>"    , bufferpos_to_prev_word   , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<C-Home>"    , bufferpos_to_buffer_start, INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<C-End>"     , bufferpos_to_buffer_end  , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOVE },
    { "<PageUp>"    , bufferpos_change_page    , INT_VAL_STRUCT(DIRECTION_UP)                           , CMDT_BUFFER_MOVE },
    { "<PageDown>"  , bufferpos_change_page    , INT_VAL_STRUCT(DIRECTION_DOWN)                         , CMDT_BUFFER_MOVE },
    { "<S-Up>"      , bufferpos_change_line    , INT_VAL_STRUCT(DIRECTION_UP    | DIRECTION_WITH_SELECT), CMDT_BUFFER_MOVE },
    { "<S-Down>"    , bufferpos_change_line    , INT_VAL_STRUCT(DIRECTION_DOWN  | DIRECTION_WITH_SELECT), CMDT_BUFFER_MOVE },
    { "<S-Right>"   , bufferpos_change_char    , INT_VAL_STRUCT(DIRECTION_RIGHT | DIRECTION_WITH_SELECT), CMDT_BUFFER_MOVE },
    { "<S-Left>"    , bufferpos_change_char    , INT_VAL_STRUCT(DIRECTION_LEFT  | DIRECTION_WITH_SELECT), CMDT_BUFFER_MOVE },
    { "<S-Home>"    , bufferpos_to_line_start  , INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<S-End>"     , bufferpos_to_line_end    , INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<C-S-Right>" , bufferpos_to_next_word   , INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<C-S-Left>"  , bufferpos_to_prev_word   , INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<C-S-Home>"  , bufferpos_to_buffer_start, INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<C-S-End>"   , bufferpos_to_buffer_end  , INT_VAL_STRUCT(DIRECTION_WITH_SELECT)                  , CMDT_BUFFER_MOVE },
    { "<S-PageUp>"  , bufferpos_change_page    , INT_VAL_STRUCT(DIRECTION_UP   | DIRECTION_WITH_SELECT) , CMDT_BUFFER_MOVE },
    { "<S-PageDown>", bufferpos_change_page    , INT_VAL_STRUCT(DIRECTION_DOWN | DIRECTION_WITH_SELECT) , CMDT_BUFFER_MOVE },
    { "<Space>"     , buffer_insert_char       , STR_VAL_STRUCT(" ")                                    , CMDT_BUFFER_MOD  }, 
    { "<Tab>"       , buffer_insert_char       , STR_VAL_STRUCT("\t")                                   , CMDT_BUFFER_MOD  }, 
    { "<KPDiv>"     , buffer_insert_char       , STR_VAL_STRUCT("/")                                    , CMDT_BUFFER_MOD  }, 
    { "<KPMult>"    , buffer_insert_char       , STR_VAL_STRUCT("*")                                    , CMDT_BUFFER_MOD  }, 
    { "<KPMinus>"   , buffer_insert_char       , STR_VAL_STRUCT("-")                                    , CMDT_BUFFER_MOD  }, 
    { "<KPPlus>"    , buffer_insert_char       , STR_VAL_STRUCT("+")                                    , CMDT_BUFFER_MOD  }, 
    { "<Delete>"    , buffer_delete_char       , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<Backspace>" , buffer_backspace         , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<Enter>"     , buffer_insert_line       , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<C-a>"       , buffer_select_all_text   , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<C-c>"       , buffer_copy_selected_text, INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<C-x>"       , buffer_cut_selected_text , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<C-v>"       , buffer_paste_text        , INT_VAL_STRUCT(0)                                      , CMDT_BUFFER_MOD  }, 
    { "<C-s>"       , buffer_save_file         , INT_VAL_STRUCT(0)                                      , CMDT_CMD_INPUT   },
    { "<M-C-Right>" , session_change_tab       , INT_VAL_STRUCT(DIRECTION_RIGHT)                        , CMDT_SESS_MOD    },
    { "<M-C-Left>"  , session_change_tab       , INT_VAL_STRUCT(DIRECTION_LEFT)                         , CMDT_SESS_MOD    },
    { "<Escape>"    , finished_processing_input, INT_VAL_STRUCT(0)                                      , CMDT_EXIT        }
};

int init_keymap(Session *sess)
{
    size_t command_num = sizeof(commands) / sizeof(Command);

    sess->keymap = new_sized_hashmap(command_num * 2);

    if (sess->keymap == NULL) {
        return 0;
    }

    Command *command;

    for (size_t k = 0; k < command_num; k++) {
        command = malloc(sizeof(Command));

        if (command == NULL) {
            return 0;
        }

        memcpy(command, &commands[k], sizeof(Command));

        if (!hashmap_set(sess->keymap, command->keystr, command)) {
            return 0;
        }
    }

    return 1;
}

static void free_command(void *command)
{
    if (command != NULL) {
        free(command);
    }
}

void free_keymap(Session *sess)
{
    if (sess == NULL || sess->keymap == NULL) {
        return;
    } 

    free_hashmap_values(sess->keymap, free_command);
    free_hashmap(sess->keymap);
}

Status do_command(Session *sess, const char *command_str, int *finished)
{
    Command *command = hashmap_get(sess->keymap, command_str);

    if (command != NULL && !command_type_excluded(sess, command->cmd_type)) {
        return command->command_handler(sess, command->param, command_str, finished);
    }

    if (!(command_str[0] == '<' && command_str[1] != '\0') &&
        !command_type_excluded(sess, CMDT_BUFFER_MOD)) {
        return insert_character(sess->active_buffer, command_str);
    }

    return STATUS_SUCCESS;
}

static Status bufferpos_change_line(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_change_line(sess->active_buffer, &sess->active_buffer->pos, param.val.ival, 1);
}

static Status bufferpos_change_char(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_change_char(sess->active_buffer, &sess->active_buffer->pos, param.val.ival, 1);
}

static Status bufferpos_to_line_start(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_line_start(sess->active_buffer, &sess->active_buffer->pos, param.val.ival & DIRECTION_WITH_SELECT, 1);
}

static Status bufferpos_to_line_end(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_line_end(sess->active_buffer, param.val.ival & DIRECTION_WITH_SELECT);
}

static Status bufferpos_to_next_word(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_next_word(sess->active_buffer, param.val.ival & DIRECTION_WITH_SELECT);
}

static Status bufferpos_to_prev_word(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_prev_word(sess->active_buffer, param.val.ival & DIRECTION_WITH_SELECT);
}

static Status bufferpos_to_buffer_start(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_buffer_start(sess->active_buffer, param.val.ival & DIRECTION_WITH_SELECT);
}

static Status bufferpos_to_buffer_end(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_to_buffer_end(sess->active_buffer, param.val.ival & DIRECTION_WITH_SELECT);
}

static Status bufferpos_change_page(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return pos_change_page(sess->active_buffer, param.val.ival);
}

static Status buffer_insert_char(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;
    return insert_character(sess->active_buffer, param.val.sval);
}

static Status buffer_delete_char(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;
    return delete_character(sess->active_buffer);
}

static Status buffer_backspace(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;

    if (!selection_started(sess->active_buffer)) {
        if (bufferpos_at_buffer_start(sess->active_buffer->pos)) {
            return STATUS_SUCCESS;
        }

        Status status = pos_change_char(sess->active_buffer, &sess->active_buffer->pos, DIRECTION_LEFT, 1);
        RETURN_IF_FAIL(status);
    }

    return delete_character(sess->active_buffer);
}

static Status buffer_insert_line(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;
    return insert_line(sess->active_buffer);
}

static Status buffer_select_all_text(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;
    return select_all_text(sess->active_buffer);
}

static Status buffer_copy_selected_text(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;

    TextSelection *text_selection;

    Status status = copy_selected_text(sess->active_buffer, &text_selection);

    if (!STATUS_IS_SUCCESS(status) || text_selection == NULL) {
        return status;
    }

    set_clipboard(sess, text_selection);

    return status;
}

static Status buffer_cut_selected_text(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;

    TextSelection *text_selection;

    Status status = cut_selected_text(sess->active_buffer, &text_selection);

    if (!STATUS_IS_SUCCESS(status) || text_selection == NULL) {
        return status;
    }

    set_clipboard(sess, text_selection);

    return status;
}

static Status buffer_paste_text(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;

    if (sess->clipboard == NULL) {
        return STATUS_SUCCESS;
    }

    return insert_textselection(sess->active_buffer, sess->clipboard);
}

static Status buffer_save_file(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;
    (void)finished;
    Buffer *buffer = sess->active_buffer;
    Status status = STATUS_SUCCESS;

    if (!has_file_path(buffer)) {
        cmd_input_prompt(sess, "Save As");

        if (sess->cmd_prompt.cancelled) {
            return STATUS_SUCCESS;
        }

        char *input = get_cmd_buffer_text(sess);

        if (input == NULL) {
            return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to process input");
        }

        if (strlen(input) == 0) {
            status = get_error(ERR_INVALID_FILE_PATH, "Invalid file path \"%s\"", 
                               input == NULL ? "NULL" : "");
        }

        if (!set_buffer_file_path(buffer, input)) {
            status = get_error(ERR_INVALID_FILE_PATH, "Out of memory - Unable to set buffer file path");
        }

        free(input);
        RETURN_IF_FAIL(status);
    }

    status = write_buffer(buffer);
    RETURN_IF_FAIL(status);

    refresh_file_attributes(&buffer->file_info);

    char msg[MAX_MSG_SIZE];
    snprintf(msg, MAX_MSG_SIZE, "Save successful: %zu lines, %zu bytes written", buffer->line_num, buffer->byte_num);
    add_msg(sess, msg);

    return status;
}

static Status session_change_tab(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)keystr;
    (void)finished;

    if (sess->buffer_num < 2) {
        return STATUS_SUCCESS;
    }

    size_t new_active_buffer_index;

    if (param.val.ival == DIRECTION_RIGHT) {
        new_active_buffer_index = (sess->active_buffer_index + 1) % sess->buffer_num;
    } else {
        if (sess->active_buffer_index == 0) {
            new_active_buffer_index = sess->buffer_num - 1; 
        } else {
            new_active_buffer_index = sess->active_buffer_index - 1;
        }
    }

    set_active_buffer(sess, new_active_buffer_index);

    return STATUS_SUCCESS;
}

static Status finished_processing_input(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)sess;
    (void)param;
    (void)keystr;

    *finished = 1;

    return STATUS_SUCCESS;
}

static Status cmd_input_prompt(Session *sess, const char *prompt_text)
{
    RETURN_IF_FAIL(make_cmd_buffer_active(sess, prompt_text));
    update_command_function(sess, "<Enter>", finished_processing_input);
    update_command_function(sess, "<Escape>", cancel_cmd_input_prompt);
    exclude_command_type(sess, CMDT_CMD_INPUT);

    update_display(sess);
    process_input(sess);

    enable_command_type(sess, CMDT_CMD_INPUT);
    update_command_function(sess, "<Enter>", buffer_insert_line);
    update_command_function(sess, "<Escape>", finished_processing_input);
    end_cmd_buffer_active(sess);

    return STATUS_SUCCESS;
}

static Status cancel_cmd_input_prompt(Session *sess, Value param, const char *keystr, int *finished)
{
    (void)param;
    (void)keystr;

    sess->cmd_prompt.cancelled = 1;
    *finished = 1;

    return STATUS_SUCCESS;
}

static int update_command_function(Session *sess, const char *keystr, CommandHandler new_command_handler)
{
    Command *command = hashmap_get(sess->keymap, keystr);

    if (command == NULL) {
        return 0;
    }

    command->command_handler = new_command_handler;

    return 1;
}

