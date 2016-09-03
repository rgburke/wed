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
#include "value.h"
#include "radix_tree.h"

struct Session;

#define MAX_CMD_ARG_NUM 5

/* Commands are in effect just functions with the same signature
 * that perform various actions in wed. This allows them to be mapped to
 * key presses and called in a generic way. Commands interact with lower level
 * entities like buffers and allow their functionality to be exposed.
 * Below is a list of all the Commands that are currently available in wed */
typedef enum {
    CMD_NOP,
    CMD_BP_CHANGE_LINE,
    CMD_BP_CHANGE_CHAR,
    CMD_BP_TO_LINE_START,
    CMD_BP_TO_HARD_LINE_START,
    CMD_BP_TO_LINE_END,
    CMD_BP_TO_HARD_LINE_END,
    CMD_BP_TO_NEXT_WORD,
    CMD_BP_TO_PREV_WORD,
    CMD_BP_TO_NEXT_PARAGRAPH,
    CMD_BP_TO_PREV_PARAGRAPH,
    CMD_BP_CHANGE_PAGE,
    CMD_BP_TO_BUFFER_START,
    CMD_BP_TO_BUFFER_END,
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
    CMD_BUFFER_JOIN_LINES,
    CMD_BUFFER_SAVE_FILE,
    CMD_BUFFER_SAVE_AS,
    CMD_BUFFER_FIND,
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
    CMD_SESSION_ECHO,
    CMD_SESSION_MAP,
    CMD_SESSION_UNMAP
} Command;

/* Operations are instances of commands i.e. they define a command with
 * arguments */
typedef enum {
    OP_NOP,
    OP_MOVE_PREV_LINE,
    OP_MOVE_NEXT_LINE,
    OP_MOVE_NEXT_CHAR,
    OP_MOVE_PREV_CHAR,
    OP_MOVE_START_OF_SCREEN_LINE,
    OP_MOVE_START_OF_LINE,
    OP_MOVE_END_OF_SCREEN_LINE,
    OP_MOVE_END_OF_LINE,
    OP_MOVE_NEXT_WORD,
    OP_MOVE_PREV_WORD,
    OP_MOVE_NEXT_PARAGRAPH,
    OP_MOVE_PREV_PARAGRAPH,
    OP_MOVE_PREV_PAGE,
    OP_MOVE_NEXT_PAGE,
    OP_MOVE_BUFFER_START,
    OP_MOVE_BUFFER_END,
    OP_MOVE_SELECT_PREV_LINE,
    OP_MOVE_SELECT_NEXT_LINE,
    OP_MOVE_SELECT_NEXT_CHAR,
    OP_MOVE_SELECT_PREV_CHAR,
    OP_MOVE_SELECT_START_OF_SCREEN_LINE,
    OP_MOVE_SELECT_START_OF_LINE,
    OP_MOVE_SELECT_END_OF_SCREEN_LINE,
    OP_MOVE_SELECT_END_OF_LINE,
    OP_MOVE_SELECT_NEXT_WORD,
    OP_MOVE_SELECT_PREV_WORD,
    OP_MOVE_SELECT_NEXT_PARAGRAPH,
    OP_MOVE_SELECT_PREV_PARAGRAPH,
    OP_MOVE_SELECT_PREV_PAGE,
    OP_MOVE_SELECT_NEXT_PAGE,
    OP_MOVE_SELECT_BUFFER_START,
    OP_MOVE_SELECT_BUFFER_END,
    OP_MOVE_MATCHING_BRACKET,
    OP_INDENT,
    OP_UNINDENT,
    OP_DELETE,
    OP_BACKSPACE,
    OP_DELETE_NEXT_WORD,
    OP_DELETE_PREV_WORD,
    OP_INSERT_NEWLINE,
    OP_INSERT_SPACE,
    OP_INSERT_KPDIV,
    OP_INSERT_KPMULT,
    OP_INSERT_KPMINUS,
    OP_INSERT_KPPLUS,
    OP_SELECT_ALL,
    OP_COPY,
    OP_CUT,
    OP_PASTE,
    OP_UNDO,
    OP_REDO,
    OP_MOVE_LINES_UP,
    OP_MOVE_LINES_DOWN,
    OP_DUPLICATE,
    OP_JOIN_LINES,
    OP_SAVE,
    OP_SAVE_AS,
    OP_FIND,
    OP_FIND_REPLACE,
    OP_GOTO_LINE,
    OP_OPEN,
    OP_NEW,
    OP_NEXT_BUFFER,
    OP_PREV_BUFFER,
    OP_SAVE_ALL,
    OP_CLOSE,
    OP_CMD,
    OP_CHANGE_BUFFER,
    OP_SUSPEND,
    OP_EXIT,
    OP_TOGGLE_SEARCH_TYPE,
    OP_TOGGLE_SEARCH_CASE_SENSITIVITY,
    OP_TOGGLE_SEARCH_DIRECTION,
    OP_PROMPT_PREV_ENTRY,
    OP_PROMPT_NEXT_ENTRY,
    OP_PROMPT_SUBMIT,
    OP_PROMPT_CANCEL,
    OP_PROMPT_COMPLETE,
    OP_PROMPT_COMPLETE_PREV
} Operation;

/* Container structure for Command arguments */
typedef struct {
    struct Session *sess; /* Commands can change global state */
    Value args[MAX_CMD_ARG_NUM]; /* Array of int|string|bool values */
    size_t arg_num; /* Number of values in args actually set */
    const char *key; /* The key press that invoked this Command */
    int *finished; /* Set equal to true will stop processing input */
} CommandArgs;

typedef Status (*CommandHandler)(const CommandArgs *);

/* Categorisation of commands */
typedef enum {
    CMDT_NOP         = 0,
    CMDT_BUFFER_MOVE = 1 << 0,
    CMDT_BUFFER_MOD  = 1 << 1,
    CMDT_CMD_INPUT   = 1 << 2,
    CMDT_EXIT        = 1 << 3,
    CMDT_SESS_MOD    = 1 << 4,
    CMDT_CMD_MOD     = 1 << 5,
    CMDT_SUSPEND     = 1 << 6
} CommandType;

/* Describes the arguments a command expects */
typedef struct {
    int is_var_args; /* True if a variable number of arguments are accepted.
                        When true other members of this struct aren't used */
    size_t arg_num; /* The number of arguments this command expects */
    ValueType arg_types[MAX_CMD_ARG_NUM]; /* Value types of the expected 
                                             arguments */
} CommandSignature;

#define CMDSIG_STRUCT(is_var_args,arg_num,...) { \
                (is_var_args), \
                (arg_num), \
                { __VA_ARGS__ } \
            }

#define CMDSIG_VAR_ARGS CMDSIG_STRUCT(1,0,VAL_TYPE_INT)
#define CMDSIG_NO_ARGS CMDSIG_STRUCT(0,0,VAL_TYPE_INT)
#define CMDSIG(arg_num,...) CMDSIG_STRUCT(0,(arg_num),__VA_ARGS__)

/* Command descriptor */
typedef struct {
    const char *function_name; /* Config function name */
    CommandHandler command_handler; /* function reference */
    CommandSignature command_signature; /* Arg info */
    CommandType command_type; /* High level categorisation of 
                                 what this command does */
} CommandDefinition;

#define CMD_NO_ARGS { INT_VAL_STRUCT(0) }

/* Modes in which key bindings can be defined */
typedef enum {
    OM_STANDARD, /* Normal buffer is open */
    OM_PROMPT, /* Prompt is open */
    OM_PROMPT_COMPLETER, /* Prompt with auto complete functionality is open */
    OM_USER, /* User defined mappings */
    OM_ENTRY_NUM
} OperationMode;

/* Stores all key bindings */
typedef struct {
    int active_op_modes[OM_ENTRY_NUM]; /* Track which modes are active */
    RadixTree *maps[OM_ENTRY_NUM]; /* Key bindings for each mode */
} KeyMap;

typedef enum {
    KMT_OPERATION,
    KMT_KEYSTR
} KeyMappingType;

/* Maps a key press to an operation. A collection of KeyMapping's defines
 * a set of key bindings */
typedef struct {
    KeyMappingType type;
    char *key; /* Key string */
    union {
        Operation op; /* Operation */
        char *keystr; /* Sequence of keys */
    } value;
} KeyMapping;

/* An operation specifies a command, arguments to the command and an operation
 * mode in which it is valid. They define actions which key's can be mapped
 * to */
typedef struct {
    const char *name; /* Operation name */
    OperationMode op_mode; /* Operation mode in which this is valid */
    Value args[MAX_CMD_ARG_NUM]; /* Arguments to Command */
    size_t arg_num; /* Argument number */
    Command command; /* The Command to be called */
} OperationDefinition;

int cm_init_key_map(KeyMap *);
void cm_free_key_map(KeyMap *);
Status cm_do_operation(struct Session *, const char *key, int *finished);
int cm_is_valid_operation(const struct Session *, const char *key,
                          size_t key_len, int *is_prefix);
Status cm_do_command(Command cmd, CommandArgs *cmd_args);
int cm_get_command(const char *function_name, Command *cmd);

#endif
