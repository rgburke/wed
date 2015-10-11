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

#ifndef WED_COMMAND_H
#define WED_COMMAND_H

#include "shared.h"
#include "status.h"
#include "session.h"
#include "value.h"

#define MAX_CMD_ARG_NUM 5

typedef enum {
    CMD_BP_CHANGE_LINE,
    CMD_BP_CHANGE_CHAR,
    CMD_BP_TO_LINE_START,
    CMD_BP_TO_LINE_END,
    CMD_BP_TO_NEXT_WORD,
    CMD_BP_TO_PREV_WORD,
    CMD_BP_TO_BUFFER_START,
    CMD_BP_TO_BUFFER_END,
    CMD_BP_CHANGE_PAGE,
    CMD_BP_GOTO_MATCHING_BRACKET,
    CMD_BUFFER_INSERT_CHAR,
    CMD_BUFFER_INDENT,
    CMD_BUFFER_DELETE_CHAR,
    CMD_BUFFER_BACKSPACE,
    CMD_BUFFER_DELETE_WORD,
    CMD_BUFFER_DELETE_PREV_WORD,
    CMD_BUFFER_INSERT_LINE,
    CMD_BUFFER_SELECT_ALL_TEXT,
    CMD_BUFFER_COPY_SELECTED_TEXT,
    CMD_BUFFER_CUT_SELECTED_TEXT,
    CMD_BUFFER_PASTE_TEXT,
    CMD_BUFFER_UNDO,
    CMD_BUFFER_REDO,
    CMD_BUFFER_VERT_MOVE_LINES,
    CMD_BUFFER_DUPLICATE_SELECTION,
    CMD_BUFFER_SAVE_FILE,
    CMD_BUFFER_FIND,
    CMD_BUFFER_FIND_NEXT,
    CMD_BUFFER_TOGGLE_SEARCH_TYPE,
    CMD_BUFFER_TOGGLE_SEARCH_CASE,
    CMD_BUFFER_TOGGLE_SEARCH_DIRECTION,
    CMD_BUFFER_REPLACE,
    CMD_PREVIOUS_PROMPT_ENTRY,
    CMD_NEXT_PROMPT_ENTRY,
    CMD_PROMPT_INPUT_FINISHED,
    CMD_CANCEL_PROMPT,
    CMD_RUN_PROMPT_COMPLETION,
    CMD_BUFFER_GOTO_LINE,
    CMD_SESSION_OPEN_FILE,
    CMD_SESSION_ADD_EMPTY_BUFFER,
    CMD_SESSION_CHANGE_TAB,
    CMD_SESSION_SAVE_ALL,
    CMD_SESSION_CLOSE_BUFFER,
    CMD_SESSION_RUN_COMMAND,
    CMD_SESSION_CHANGE_BUFFER,
    CMD_SUSPEND,
    CMD_SESSION_END,
} Command;

typedef struct {
    Session *sess;
    Value args[MAX_CMD_ARG_NUM];
    size_t arg_num;
    const char *key;
    int *finished;
} CommandArgs;

typedef Status (*CommandHandler)(const CommandArgs *);

typedef struct {
    CommandHandler command_handler;
    CommandType cmd_type;
} CommandDefinition;

typedef enum {
    OM_STANDARD = 1,
    OM_PROMPT = 1 << 1,
    OM_PROMPT_COMPLETER = 1 << 2
} OperationMode;

typedef struct {
    const char *key;
    OperationMode op_mode;
    Value args[MAX_CMD_ARG_NUM];
    size_t arg_num;
    Command command;
} Operation;

int cm_populate_keymap(HashMap *, OperationMode);
void cm_clear_keymap_entries(HashMap *);
Status cm_do_operation(Session *, const char *, int *);

#endif
