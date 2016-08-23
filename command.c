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
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "shared.h"
#include "status.h"
#include "command.h"
#include "session.h"
#include "buffer.h"
#include "value.h"
#include "util.h"
#include "input.h"
#include "config.h"
#include "config_parse_util.h"
#include "search.h"
#include "replace.h"
#include "prompt_completer.h"

/* Used for Yes/No type prompt questions
 * e.g. Do you want to save file? */
typedef enum {
    QR_NONE = 0,
    QR_YES = 1,
    QR_NO = 1 << 1,
    QR_CANCEL = 1 << 2,
    QR_ERROR = 1 << 3,
    QR_ALL = 1 << 4
} QuestionRespose;

static KeyMapping *cm_new_op_key_mapping(const char *key, Operation);
static KeyMapping *cm_new_keystr_key_mapping(const char *key,
                                             const char *keystr);
static KeyMapping *cm_new_key_mapping(KeyMappingType, const char *key,
                                      Operation, const char *keystr);
static void cm_free_key_mapping(KeyMapping *);
static Status cm_run_command(const CommandDefinition *, CommandArgs *);

static Status cm_nop(const CommandArgs *);
static Status cm_bp_change_line(const CommandArgs *);
static Status cm_bp_change_char(const CommandArgs *); 
static Status cm_bp_to_line_start(const CommandArgs *);
static Status cm_bp_to_hard_line_start(const CommandArgs *);
static Status cm_bp_to_line_end(const CommandArgs *);
static Status cm_bp_to_hard_line_end(const CommandArgs *);
static Status cm_bp_to_next_word(const CommandArgs *);
static Status cm_bp_to_prev_word(const CommandArgs *);
static Status cm_bp_to_buffer_start(const CommandArgs *);
static Status cm_bp_to_buffer_end(const CommandArgs *);
static Status cm_bp_change_page(const CommandArgs *);
static Status cm_bp_to_next_paragraph(const CommandArgs *);
static Status cm_bp_to_prev_paragraph(const CommandArgs *);
static Status cm_bp_goto_matching_bracket(const CommandArgs *);
static Status cm_buffer_insert_char(const CommandArgs *);
static Status cm_buffer_delete_char(const CommandArgs *);
static Status cm_buffer_backspace(const CommandArgs *);
static Status cm_buffer_delete_word(const CommandArgs *);
static Status cm_buffer_delete_prev_word(const CommandArgs *);
static Status cm_buffer_insert_line(const CommandArgs *);
static Status cm_buffer_select_all_text(const CommandArgs *);
static Status cm_buffer_copy_selected_text(const CommandArgs *);
static Status cm_buffer_cut_selected_text(const CommandArgs *);
static Status cm_buffer_paste_text(const CommandArgs *);
static Status cm_buffer_undo(const CommandArgs *);
static Status cm_buffer_redo(const CommandArgs *);
static Status cm_buffer_vert_move_lines(const CommandArgs *);
static Status cm_buffer_duplicate_selection(const CommandArgs *);
static Status cm_buffer_join_lines(const CommandArgs *);
static Status cm_buffer_indent(const CommandArgs *);
static Status cm_buffer_save_file(const CommandArgs *);
static Status cm_buffer_save_as(const CommandArgs *);
static Status cm_save_file_prompt(Session *, char **file_path_ptr);
static void cm_generate_find_prompt(const BufferSearch *,
                                    char prompt_text[MAX_CMD_PROMPT_LENGTH]);
static Status cm_prepare_search(Session *, const BufferPos *start_pos);
static Status cm_buffer_find(const CommandArgs *);
static Status cm_buffer_find_next(const CommandArgs *);
static Status cm_buffer_toggle_search_direction(const CommandArgs *);
static Status cm_buffer_toggle_search_type(const CommandArgs *);
static Status cm_buffer_toggle_search_case(const CommandArgs *);
static Status cm_buffer_replace(const CommandArgs *);
static Status cm_buffer_goto_line(const CommandArgs *);
static Status cm_prepare_replace(Session *, char **rep_text_ptr,
                                 size_t *rep_length);
static Status cm_session_open_file(const CommandArgs *);
static Status cm_session_add_empty_buffer(const CommandArgs *);
static Status cm_session_change_tab(const CommandArgs *);
static Status cm_session_save_all(const CommandArgs *);
static Status cm_session_close_buffer(const CommandArgs *);
static Status cm_session_run_command(const CommandArgs *);
static Status cm_previous_prompt_entry(const CommandArgs *);
static Status cm_next_prompt_entry(const CommandArgs *);
static Status cm_prompt_input_finished(const CommandArgs *);
static Status cm_session_change_buffer(const CommandArgs *);
static Status cm_determine_buffer(Session *, const char *input, 
                                  const Buffer **buffer_ptr);
static Status cm_suspend(const CommandArgs *);
static Status cm_session_end(const CommandArgs *);

static Status cm_cmd_input_prompt(Session *, PromptType,
                                  const char *prompt_text, List *history,
                                  int show_last_cmd);
static QuestionRespose cm_question_prompt(Session *, PromptType,
                                          const char *question,
                                          QuestionRespose allowed_answers,
                                          QuestionRespose default_answer);
static Status cm_cancel_prompt(const CommandArgs *);
static Status cm_run_prompt_completion(const CommandArgs *);
static Status cm_session_echo(const CommandArgs *);
static Status cm_session_map(const CommandArgs *);
static Status cm_session_unmap(const CommandArgs *);

/* Allow the following to exceed 80 columns.
 * This format is easier to read and maipulate in visual block mode in vim */
static const CommandDefinition cm_commands[] = {
    [CMD_NOP]                            = { NULL    , cm_nop                           , CMDSIG_NO_ARGS                       , CMDT_NOP         },
    [CMD_BP_CHANGE_LINE]                 = { NULL    , cm_bp_change_line                , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_CHANGE_CHAR]                 = { NULL    , cm_bp_change_char                , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_LINE_START]               = { NULL    , cm_bp_to_line_start              , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_HARD_LINE_START]          = { NULL    , cm_bp_to_hard_line_start         , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_LINE_END]                 = { NULL    , cm_bp_to_line_end                , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_HARD_LINE_END]            = { NULL    , cm_bp_to_hard_line_end           , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_NEXT_WORD]                = { NULL    , cm_bp_to_next_word               , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_PREV_WORD]                = { NULL    , cm_bp_to_prev_word               , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_NEXT_PARAGRAPH]           = { NULL    , cm_bp_to_next_paragraph          , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_PREV_PARAGRAPH]           = { NULL    , cm_bp_to_prev_paragraph          , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_CHANGE_PAGE]                 = { NULL    , cm_bp_change_page                , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_BUFFER_START]             = { NULL    , cm_bp_to_buffer_start            , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_TO_BUFFER_END]               = { NULL    , cm_bp_to_buffer_end              , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOVE },
    [CMD_BP_GOTO_MATCHING_BRACKET]       = { NULL    , cm_bp_goto_matching_bracket      , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOVE },
    [CMD_BUFFER_INSERT_CHAR]             = { NULL    , cm_buffer_insert_char            , CMDSIG(1, VAL_TYPE_STR)              , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_INDENT]                  = { NULL    , cm_buffer_indent                 , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_DELETE_CHAR]             = { NULL    , cm_buffer_delete_char            , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_BACKSPACE]               = { NULL    , cm_buffer_backspace              , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_DELETE_WORD]             = { NULL    , cm_buffer_delete_word            , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_DELETE_PREV_WORD]        = { NULL    , cm_buffer_delete_prev_word       , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_INSERT_LINE]             = { NULL    , cm_buffer_insert_line            , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_SELECT_ALL_TEXT]         = { NULL    , cm_buffer_select_all_text        , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_COPY_SELECTED_TEXT]      = { NULL    , cm_buffer_copy_selected_text     , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_CUT_SELECTED_TEXT]       = { NULL    , cm_buffer_cut_selected_text      , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_PASTE_TEXT]              = { NULL    , cm_buffer_paste_text             , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_UNDO]                    = { NULL    , cm_buffer_undo                   , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_REDO]                    = { NULL    , cm_buffer_redo                   , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_VERT_MOVE_LINES]         = { NULL    , cm_buffer_vert_move_lines        , CMDSIG(1, VAL_TYPE_INT)              , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_DUPLICATE_SELECTION]     = { NULL    , cm_buffer_duplicate_selection    , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_JOIN_LINES]              = { NULL    , cm_buffer_join_lines             , CMDSIG_NO_ARGS                       , CMDT_BUFFER_MOD  },
    [CMD_BUFFER_SAVE_FILE]               = { NULL    , cm_buffer_save_file              , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_BUFFER_SAVE_AS]                 = { NULL    , cm_buffer_save_as                , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_BUFFER_FIND]                    = { NULL    , cm_buffer_find                   , CMDSIG(1, VAL_TYPE_INT)              , CMDT_CMD_INPUT   },
    [CMD_BUFFER_FIND_NEXT]               = { NULL    , cm_buffer_find_next              , CMDSIG(1, VAL_TYPE_INT)              , CMDT_CMD_INPUT   },
    [CMD_BUFFER_TOGGLE_SEARCH_TYPE]      = { NULL    , cm_buffer_toggle_search_type     , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_BUFFER_TOGGLE_SEARCH_CASE]      = { NULL    , cm_buffer_toggle_search_case     , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_BUFFER_TOGGLE_SEARCH_DIRECTION] = { NULL    , cm_buffer_toggle_search_direction, CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_BUFFER_REPLACE]                 = { NULL    , cm_buffer_replace                , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_PREVIOUS_PROMPT_ENTRY]          = { NULL    , cm_previous_prompt_entry         , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_NEXT_PROMPT_ENTRY]              = { NULL    , cm_next_prompt_entry             , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_PROMPT_INPUT_FINISHED]          = { NULL    , cm_prompt_input_finished         , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_CANCEL_PROMPT]                  = { NULL    , cm_cancel_prompt                 , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_RUN_PROMPT_COMPLETION]          = { NULL    , cm_run_prompt_completion         , CMDSIG_NO_ARGS                       , CMDT_CMD_MOD     },
    [CMD_BUFFER_GOTO_LINE]               = { NULL    , cm_buffer_goto_line              , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_SESSION_OPEN_FILE]              = { NULL    , cm_session_open_file             , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_SESSION_ADD_EMPTY_BUFFER]       = { NULL    , cm_session_add_empty_buffer      , CMDSIG_NO_ARGS                       , CMDT_SESS_MOD    },
    [CMD_SESSION_CHANGE_TAB]             = { NULL    , cm_session_change_tab            , CMDSIG(1, VAL_TYPE_INT)              , CMDT_SESS_MOD    },
    [CMD_SESSION_SAVE_ALL]               = { NULL    , cm_session_save_all              , CMDSIG_NO_ARGS                       , CMDT_SESS_MOD    },
    [CMD_SESSION_CLOSE_BUFFER]           = { NULL    , cm_session_close_buffer          , CMDSIG(1, VAL_TYPE_INT)              , CMDT_CMD_INPUT   },
    [CMD_SESSION_RUN_COMMAND]            = { NULL    , cm_session_run_command           , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_SESSION_CHANGE_BUFFER]          = { NULL    , cm_session_change_buffer         , CMDSIG_NO_ARGS                       , CMDT_CMD_INPUT   },
    [CMD_SUSPEND]                        = { NULL    , cm_suspend                       , CMDSIG_NO_ARGS                       , CMDT_SUSPEND     },
    [CMD_SESSION_END]                    = { NULL    , cm_session_end                   , CMDSIG_NO_ARGS                       , CMDT_EXIT        },
    /* Commands that by default are not mapped to key bindings and are instead exposed as functions */
    [CMD_SESSION_ECHO]                   = { "echo"  , cm_session_echo                  , CMDSIG_VAR_ARGS                      , CMDT_SESS_MOD    },
    [CMD_SESSION_MAP]                    = { "map"   , cm_session_map                   , CMDSIG(2, VAL_TYPE_STR, VAL_TYPE_STR), CMDT_SESS_MOD    },
    [CMD_SESSION_UNMAP]                  = { "unmap" , cm_session_unmap                 , CMDSIG(1, VAL_TYPE_STR)              , CMDT_SESS_MOD    }
};

static const OperationDefinition cm_operations[] = {
    [OP_NOP]            = { "<wed-nop>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_NOP },
    [OP_MOVE_PREV_LINE] = { "<wed-move-prev-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_UP) }, 1, CMD_BP_CHANGE_LINE },
    [OP_MOVE_NEXT_LINE] = { "<wed-move-next-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_DOWN) }, 1, CMD_BP_CHANGE_LINE },
    [OP_MOVE_NEXT_CHAR] = { "<wed-move-next-char>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_RIGHT) }, 1, CMD_BP_CHANGE_CHAR },
    [OP_MOVE_PREV_CHAR] = { "<wed-move-prev-char>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_LEFT) }, 1, CMD_BP_CHANGE_CHAR },
    [OP_MOVE_START_OF_SCREEN_LINE] = { "<wed-move-start-of-screen-line>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_LINE_START },
    [OP_MOVE_START_OF_LINE] = { "<wed-move-start-of-line>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_HARD_LINE_START },
    [OP_MOVE_END_OF_SCREEN_LINE] = { "<wed-move-end-of-screen-line>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_LINE_END },
    [OP_MOVE_END_OF_LINE] = { "<wed-move-end-of-line>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_HARD_LINE_END },
    [OP_MOVE_NEXT_WORD] = { "<wed-move-next-word>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_NEXT_WORD },
    [OP_MOVE_PREV_WORD] = { "<wed-move-prev-word>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_PREV_WORD },
    [OP_MOVE_PREV_PARAGRAPH] = { "<wed-move-prev-paragraph>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_PREV_PARAGRAPH },
    [OP_MOVE_NEXT_PARAGRAPH] = { "<wed-move-next-paragraph>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_NEXT_PARAGRAPH },
    [OP_MOVE_PREV_PAGE] = { "<wed-move-prev-page>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_UP) }, 1, CMD_BP_CHANGE_PAGE },
    [OP_MOVE_NEXT_PAGE] = { "<wed-move-next-page>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_DOWN) }, 1, CMD_BP_CHANGE_PAGE },
    [OP_MOVE_BUFFER_START] = { "<wed-move-buffer-start>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_BUFFER_START },
    [OP_MOVE_BUFFER_END] = { "<wed-move-buffer-end>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BP_TO_BUFFER_END },
    [OP_MOVE_SELECT_PREV_LINE] = { "<wed-move-select-prev-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_UP | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_LINE },
    [OP_MOVE_SELECT_NEXT_LINE] = { "<wed-move-select-next-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_DOWN | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_LINE },
    [OP_MOVE_SELECT_NEXT_CHAR] = { "<wed-move-select-next-char>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_RIGHT | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_CHAR },
    [OP_MOVE_SELECT_PREV_CHAR] = { "<wed-move-select-prev-char>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_LEFT | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_CHAR },
    [OP_MOVE_SELECT_START_OF_SCREEN_LINE] = { "<wed-move-select-start-of-screen-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_LINE_START },
    [OP_MOVE_SELECT_START_OF_LINE] = { "<wed-move-select-start-of-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_HARD_LINE_START },
    [OP_MOVE_SELECT_END_OF_SCREEN_LINE] = { "<wed-move-select-end-of-screen-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_LINE_END },
    [OP_MOVE_SELECT_END_OF_LINE] = { "<wed-move-select-end-of-line>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_HARD_LINE_END },
    [OP_MOVE_SELECT_NEXT_WORD] = { "<wed-move-select-next-word>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_NEXT_WORD },
    [OP_MOVE_SELECT_PREV_WORD] = { "<wed-move-select-prev-word>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_PREV_WORD },
    [OP_MOVE_SELECT_PREV_PARAGRAPH] = { "<wed-move-select-prev-paragraph>", OM_STANDARD, { INT_VAL_STRUCT(1) }, 1, CMD_BP_TO_PREV_PARAGRAPH },
    [OP_MOVE_SELECT_NEXT_PARAGRAPH] = { "<wed-move-select-next-paragraph>", OM_STANDARD, { INT_VAL_STRUCT(1) }, 1, CMD_BP_TO_NEXT_PARAGRAPH },
    [OP_MOVE_SELECT_PREV_PAGE] = { "<wed-move-select-prev-page>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_UP | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_PAGE },
    [OP_MOVE_SELECT_NEXT_PAGE] = { "<wed-move-select-next-page>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_DOWN | DIRECTION_WITH_SELECT) }, 1, CMD_BP_CHANGE_PAGE },
    [OP_MOVE_SELECT_BUFFER_START] = { "<wed-move-select-buffer-start>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_BUFFER_START },
    [OP_MOVE_SELECT_BUFFER_END] = { "<wed-move-select-buffer-end>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_WITH_SELECT) }, 1, CMD_BP_TO_BUFFER_END },
    [OP_MOVE_MATCHING_BRACKET] = { "<wed-move-matching-bracket>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BP_GOTO_MATCHING_BRACKET },
    [OP_INDENT] = { "<wed-indent>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_RIGHT) }, 1, CMD_BUFFER_INDENT },
    [OP_UNINDENT] = { "<wed-unindent>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_LEFT) }, 1, CMD_BUFFER_INDENT },
    [OP_DELETE] = { "<wed-delete>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_DELETE_CHAR },
    [OP_BACKSPACE] = { "<wed-backspace>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_BACKSPACE },
    [OP_DELETE_NEXT_WORD] = { "<wed-delete-next-word>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_DELETE_WORD },
    [OP_DELETE_PREV_WORD] = { "<wed-delete-prev-word>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_DELETE_PREV_WORD },
    [OP_INSERT_NEWLINE] = { "<wed-insert-newline>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_INSERT_LINE },
    [OP_INSERT_SPACE] = { "<wed-insert-space>", OM_STANDARD, { STR_VAL_STRUCT(" ") }, 1, CMD_BUFFER_INSERT_CHAR },
    [OP_INSERT_KPDIV] = { "<wed-insert-kpdiv>", OM_STANDARD, { STR_VAL_STRUCT("/") }, 1, CMD_BUFFER_INSERT_CHAR },
    [OP_INSERT_KPMULT] = { "<wed-insert-kpmult>", OM_STANDARD, { STR_VAL_STRUCT("*") }, 1, CMD_BUFFER_INSERT_CHAR },
    [OP_INSERT_KPMINUS] = { "<wed-insert-kpminus>", OM_STANDARD, { STR_VAL_STRUCT("-") }, 1, CMD_BUFFER_INSERT_CHAR },
    [OP_INSERT_KPPLUS] = { "<wed-insert-kpplus>", OM_STANDARD, { STR_VAL_STRUCT("+") }, 1, CMD_BUFFER_INSERT_CHAR },
    [OP_SELECT_ALL] = { "<wed-select-all>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_SELECT_ALL_TEXT },
    [OP_COPY] = { "<wed-copy>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_COPY_SELECTED_TEXT },
    [OP_CUT] = { "<wed-cut>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_CUT_SELECTED_TEXT },
    [OP_PASTE] = { "<wed-paste>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_PASTE_TEXT },
    [OP_UNDO] = { "<wed-undo>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_UNDO },
    [OP_REDO] = { "<wed-redo>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_REDO },
    [OP_MOVE_LINES_UP] = { "<wed-move-lines-up>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_UP) }, 1, CMD_BUFFER_VERT_MOVE_LINES },
    [OP_MOVE_LINES_DOWN] = { "<wed-move-lines-down>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_DOWN) }, 1, CMD_BUFFER_VERT_MOVE_LINES },
    [OP_DUPLICATE] = { "<wed-duplicate>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_DUPLICATE_SELECTION },
    [OP_JOIN_LINES] = { "<wed-join-lines>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_JOIN_LINES },
    [OP_SAVE] = { "<wed-save>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_SAVE_FILE },
    [OP_SAVE_AS] = { "<wed-save-as>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_SAVE_AS },
    [OP_FIND] = { "<wed-find>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BUFFER_FIND },
    [OP_FIND_NEXT] = { "<wed-find-next>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_BUFFER_FIND_NEXT },
    [OP_FIND_PREV] = { "<wed-find-prev>", OM_STANDARD, { INT_VAL_STRUCT(1) }, 1, CMD_BUFFER_FIND_NEXT },
    [OP_FIND_REPLACE] = { "<wed-find-replace>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_REPLACE },
    [OP_GOTO_LINE] = { "<wed-goto-line>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_BUFFER_GOTO_LINE },
    [OP_OPEN] = { "<wed-open>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_OPEN_FILE },
    [OP_NEW] = { "<wed-new>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_ADD_EMPTY_BUFFER },
    [OP_NEXT_BUFFER] = { "<wed-next-buffer>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_RIGHT) }, 1, CMD_SESSION_CHANGE_TAB },
    [OP_PREV_BUFFER] = { "<wed-prev-buffer>", OM_STANDARD, { INT_VAL_STRUCT(DIRECTION_LEFT) }, 1, CMD_SESSION_CHANGE_TAB },
    [OP_SAVE_ALL] = { "<wed-save-all>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_SAVE_ALL },
    [OP_CLOSE] = { "<wed-close>", OM_STANDARD, { INT_VAL_STRUCT(0) }, 1, CMD_SESSION_CLOSE_BUFFER },
    [OP_CMD] = { "<wed-cmd>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_RUN_COMMAND },
    [OP_CHANGE_BUFFER] = { "<wed-change-buffer>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_CHANGE_BUFFER },
    [OP_SUSPEND] = { "<wed-suspend>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SUSPEND },
    [OP_EXIT] = { "<wed-exit>", OM_STANDARD, CMD_NO_ARGS, 0, CMD_SESSION_END },
    [OP_TOGGLE_SEARCH_TYPE] = { "<wed-toggle-search-type>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_BUFFER_TOGGLE_SEARCH_TYPE },
    [OP_TOGGLE_SEARCH_CASE_SENSITIVITY] = { "<wed-toggle-search-case-sensitivity>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_BUFFER_TOGGLE_SEARCH_CASE },
    [OP_TOGGLE_SEARCH_DIRECTION] = { "<wed-toggle-search-direction>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_BUFFER_TOGGLE_SEARCH_DIRECTION },
    [OP_PROMPT_PREV_ENTRY] = { "<wed-prompt-prev-entry>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_PREVIOUS_PROMPT_ENTRY },
    [OP_PROMPT_NEXT_ENTRY] = { "<wed-prompt-next-entry>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_NEXT_PROMPT_ENTRY },
    [OP_PROMPT_SUBMIT] = { "<wed-prompt-submit>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_PROMPT_INPUT_FINISHED },
    [OP_PROMPT_CANCEL] = { "<wed-prompt-cancel>", OM_PROMPT, CMD_NO_ARGS, 0, CMD_CANCEL_PROMPT },
    [OP_PROMPT_COMPLETE] = { "<wed-prompt-complete>", OM_PROMPT_COMPLETER, CMD_NO_ARGS, 0, CMD_RUN_PROMPT_COMPLETION },
    [OP_PROMPT_COMPLETE_PREV] = { "<wed-prompt-complete-prev>", OM_PROMPT_COMPLETER, CMD_NO_ARGS, 0, CMD_RUN_PROMPT_COMPLETION }
};

/* Default wed keybindings */
static const KeyMapping cm_key_mappings[] = {
    { KMT_OPERATION, "<Up>",          { OP_MOVE_PREV_LINE                   } },
    { KMT_OPERATION, "<Down>",        { OP_MOVE_NEXT_LINE                   } },
    { KMT_OPERATION, "<Right>",       { OP_MOVE_NEXT_CHAR                   } },
    { KMT_OPERATION, "<Left>",        { OP_MOVE_PREV_CHAR                   } },
    { KMT_OPERATION, "<Home>",        { OP_MOVE_START_OF_SCREEN_LINE        } },
    { KMT_OPERATION, "<M-Home>",      { OP_MOVE_START_OF_LINE               } },
    { KMT_OPERATION, "<End>",         { OP_MOVE_END_OF_SCREEN_LINE          } },
    { KMT_OPERATION, "<M-End>",       { OP_MOVE_END_OF_LINE                 } },
    { KMT_OPERATION, "<C-Right>",     { OP_MOVE_NEXT_WORD                   } },
    { KMT_OPERATION, "<C-Left>",      { OP_MOVE_PREV_WORD                   } },
    { KMT_OPERATION, "<C-Up>",        { OP_MOVE_PREV_PARAGRAPH              } },
    { KMT_OPERATION, "<C-Down>",      { OP_MOVE_NEXT_PARAGRAPH              } },
    { KMT_OPERATION, "<PageUp>",      { OP_MOVE_PREV_PAGE                   } },
    { KMT_OPERATION, "<PageDown>",    { OP_MOVE_NEXT_PAGE                   } },
    { KMT_OPERATION, "<C-Home>",      { OP_MOVE_BUFFER_START                } },
    { KMT_OPERATION, "<C-End>",       { OP_MOVE_BUFFER_END                  } },
    { KMT_OPERATION, "<S-Up>",        { OP_MOVE_SELECT_PREV_LINE            } },
    { KMT_OPERATION, "<S-Down>",      { OP_MOVE_SELECT_NEXT_LINE            } },
    { KMT_OPERATION, "<S-Right>",     { OP_MOVE_SELECT_NEXT_CHAR            } },
    { KMT_OPERATION, "<S-Left>",      { OP_MOVE_SELECT_PREV_CHAR            } },
    { KMT_OPERATION, "<S-Home>",      { OP_MOVE_SELECT_START_OF_SCREEN_LINE } },
    { KMT_OPERATION, "<M-S-Home>",    { OP_MOVE_SELECT_START_OF_LINE        } },
    { KMT_OPERATION, "<S-End>",       { OP_MOVE_SELECT_END_OF_SCREEN_LINE   } },
    { KMT_OPERATION, "<M-S-End>",     { OP_MOVE_SELECT_END_OF_LINE          } },
    { KMT_OPERATION, "<C-S-Right>",   { OP_MOVE_SELECT_NEXT_WORD            } },
    { KMT_OPERATION, "<C-S-Left>",    { OP_MOVE_SELECT_PREV_WORD            } },
    { KMT_OPERATION, "<C-S-Up>",      { OP_MOVE_SELECT_PREV_PARAGRAPH       } },
    { KMT_OPERATION, "<C-S-Down>",    { OP_MOVE_SELECT_NEXT_PARAGRAPH       } },
    { KMT_OPERATION, "<S-PageUp>",    { OP_MOVE_SELECT_PREV_PAGE            } },
    { KMT_OPERATION, "<S-PageDown>",  { OP_MOVE_SELECT_NEXT_PAGE            } },
    { KMT_OPERATION, "<C-S-Home>",    { OP_MOVE_SELECT_BUFFER_START         } },
    { KMT_OPERATION, "<C-S-End>",     { OP_MOVE_SELECT_BUFFER_END           } },
    { KMT_OPERATION, "<C-b>",         { OP_MOVE_MATCHING_BRACKET            } },
    { KMT_OPERATION, "<Tab>",         { OP_INDENT                           } },
    { KMT_OPERATION, "<S-Tab>",       { OP_UNINDENT                         } },
    { KMT_OPERATION, "<Delete>",      { OP_DELETE                           } },
    { KMT_OPERATION, "<Backspace>",   { OP_BACKSPACE                        } },
    { KMT_OPERATION, "<C-Delete>",    { OP_DELETE_NEXT_WORD                 } },
    { KMT_OPERATION, "<M-Backspace>", { OP_DELETE_PREV_WORD                 } },
    { KMT_OPERATION, "<Enter>",       { OP_INSERT_NEWLINE                   } },
    { KMT_OPERATION, "<Space>",       { OP_INSERT_SPACE                     } },
    { KMT_OPERATION, "<KPDiv>",       { OP_INSERT_KPDIV                     } },
    { KMT_OPERATION, "<KPMult>",      { OP_INSERT_KPMULT                    } },
    { KMT_OPERATION, "<KPMinus>",     { OP_INSERT_KPMINUS                   } },
    { KMT_OPERATION, "<KPPlus>",      { OP_INSERT_KPPLUS                    } },
    { KMT_OPERATION, "<C-a>",         { OP_SELECT_ALL                       } },
    { KMT_OPERATION, "<C-c>",         { OP_COPY                             } },
    { KMT_OPERATION, "<C-x>",         { OP_CUT                              } },
    { KMT_OPERATION, "<C-v>",         { OP_PASTE                            } },
    { KMT_OPERATION, "<C-z>",         { OP_UNDO                             } },
    { KMT_OPERATION, "<C-y>",         { OP_REDO                             } },
    { KMT_OPERATION, "<M-C-Up>",      { OP_MOVE_LINES_UP                    } },
    { KMT_OPERATION, "<M-C-Down>",    { OP_MOVE_LINES_DOWN                  } },
    { KMT_OPERATION, "<C-d>",         { OP_DUPLICATE                        } },
    { KMT_OPERATION, "<C-j>",         { OP_JOIN_LINES                       } },
    { KMT_OPERATION, "<C-s>",         { OP_SAVE                             } },
    { KMT_OPERATION, "<M-C-s>",       { OP_SAVE_AS                          } },
    { KMT_OPERATION, "<C-f>",         { OP_FIND                             } },
    { KMT_OPERATION, "<F3>",          { OP_FIND_NEXT                        } },
    { KMT_OPERATION, "<F15>",         { OP_FIND_PREV                        } },
    { KMT_OPERATION, "<C-h>",         { OP_FIND_REPLACE                     } },
    { KMT_OPERATION, "<C-r>",         { OP_FIND_REPLACE                     } },
    { KMT_OPERATION, "<C-g>",         { OP_GOTO_LINE                        } },
    { KMT_OPERATION, "<C-o>",         { OP_OPEN                             } },
    { KMT_OPERATION, "<C-n>",         { OP_NEW                              } },
    { KMT_OPERATION, "<M-C-Right>",   { OP_NEXT_BUFFER                      } },
    { KMT_OPERATION, "<M-Right>",     { OP_NEXT_BUFFER                      } },
    { KMT_OPERATION, "<M-C-Left>",    { OP_PREV_BUFFER                      } },
    { KMT_OPERATION, "<M-Left>",      { OP_PREV_BUFFER                      } },
    { KMT_OPERATION, "<C-^>",         { OP_SAVE_ALL                         } },
    { KMT_OPERATION, "<C-w>",         { OP_CLOSE                            } },
    { KMT_OPERATION, "<C-\\>",        { OP_CMD                              } },
    { KMT_OPERATION, "<C-_>",         { OP_CHANGE_BUFFER                    } },
    { KMT_OPERATION, "<M-z>",         { OP_SUSPEND                          } },
    { KMT_OPERATION, "<Escape>",      { OP_EXIT                             } },
    { KMT_OPERATION, "<C-t>",         { OP_TOGGLE_SEARCH_TYPE               } },
    { KMT_OPERATION, "<C-s>",         { OP_TOGGLE_SEARCH_CASE_SENSITIVITY   } },
    { KMT_OPERATION, "<C-d>",         { OP_TOGGLE_SEARCH_DIRECTION          } },
    { KMT_OPERATION, "<Up>",          { OP_PROMPT_PREV_ENTRY                } },
    { KMT_OPERATION, "<Down>",        { OP_PROMPT_NEXT_ENTRY                } },
    { KMT_OPERATION, "<Enter>",       { OP_PROMPT_SUBMIT                    } },
    { KMT_OPERATION, "<Escape>",      { OP_PROMPT_CANCEL                    } },
    { KMT_OPERATION, "<Tab>",         { OP_PROMPT_COMPLETE                  } },
    { KMT_OPERATION, "<S-Tab>",       { OP_PROMPT_COMPLETE_PREV             } }
};

/* Map key presses to operations. User input can be used to look
 * up an operation in a key_map so that it can be invoked */
int cm_init_key_map(KeyMap *key_map)
{
    memset(key_map, 0, sizeof(KeyMap));

    const size_t key_mapping_num = ARRAY_SIZE(cm_key_mappings, KeyMapping);

    for (size_t k = 0; k < OM_ENTRY_NUM; k++) {
        key_map->maps[k] = rt_new();

        if (key_map->maps[k] == NULL) {
            return 0;
        }
    }

    KeyMapping *key_mapping;
    const OperationDefinition *operation;
    RadixTree *map;

    for (size_t k = 0; k < key_mapping_num; k++) {
        key_mapping = (KeyMapping *)&cm_key_mappings[k];

        key_mapping = cm_new_op_key_mapping(key_mapping->key,
                                            key_mapping->value.op);

        if (key_mapping == NULL) {
            return 0;
        }

        operation = &cm_operations[key_mapping->value.op];
        map = key_map->maps[operation->op_mode];

        if (!rt_insert(map, key_mapping->key, strlen(key_mapping->key),
                       key_mapping)) {
            return 0;
        }
    }

    const size_t operation_num = ARRAY_SIZE(cm_operations, OperationDefinition);

    for (size_t k = 0; k < operation_num; k++) {
        operation = &cm_operations[k];        

        key_mapping = cm_new_op_key_mapping(operation->name, k);

        if (key_mapping == NULL) {
            return 0;
        }

        map = key_map->maps[operation->op_mode];

        if (!rt_insert(map, key_mapping->key, strlen(key_mapping->key),
                       key_mapping)) {
            return 0;
        }
    }

    key_map->active_op_modes[OM_STANDARD] = 1;

    return 1;
}

void cm_free_key_map(KeyMap *key_map)
{
    if (key_map == NULL) {
        return;
    } 

    for (size_t k = 0; k < OM_ENTRY_NUM; k++) {
        rt_free_including_entries(key_map->maps[k],
                                  (FreeFunction)cm_free_key_mapping);
    }
}

static KeyMapping *cm_new_op_key_mapping(const char *key, Operation operation)
{
    return cm_new_key_mapping(KMT_OPERATION, key, operation, NULL);
}

static KeyMapping *cm_new_keystr_key_mapping(const char *key,
                                             const char *keystr)
{
    return cm_new_key_mapping(KMT_KEYSTR, key, 0, keystr);
}

static KeyMapping *cm_new_key_mapping(KeyMappingType type, const char *key,
                                      Operation operation, const char *keystr)
{
    KeyMapping *key_mapping = malloc(sizeof(KeyMapping));

    if (key_mapping == NULL) {
        return NULL;
    }

    memset(key_mapping, 0, sizeof(KeyMapping));

    key_mapping->key = strdup(key);

    if (key_mapping->key == NULL) {
        cm_free_key_mapping(key_mapping);
        return NULL;
    }

    if (type == KMT_OPERATION) {
        key_mapping->value.op = operation;
    } else if (type == KMT_KEYSTR) {
        key_mapping->value.keystr = strdup(keystr);

        if (key_mapping->value.keystr == NULL) {
            cm_free_key_mapping(key_mapping);
            return NULL;
        }
    } else {
        assert(!"Invalid KeyMappingType");
    }

    key_mapping->type = type;

    return key_mapping;
}

static void cm_free_key_mapping(KeyMapping *key_mapping)
{
    if (key_mapping == NULL) {
        return;
    }

    if (key_mapping->type == KMT_KEYSTR) {
        free(key_mapping->value.keystr);
    }

    free(key_mapping->key);
    free(key_mapping);
}

Status cm_do_operation(Session *sess, const char *key, int *finished)
{
    assert(!is_null_or_empty(key));
    assert(finished != NULL);

    const KeyMapping *key_mapping = NULL;
    const KeyMap *key_map = &sess->key_map;
    const RadixTree *map;

    for (int k = OM_ENTRY_NUM - 1; k > -1; k--) {
        if (key_map->active_op_modes[k]) {
            map = key_map->maps[k];

            if (rt_find(map, key, strlen(key), (void **)&key_mapping, NULL)) {
                break;
            }
        }
    }

    if (key_mapping != NULL) {
        if (key_mapping->type == KMT_OPERATION) {
            const OperationDefinition *operation =
                &cm_operations[key_mapping->value.op];
            const CommandDefinition *command = &cm_commands[operation->command];

            CommandArgs cmd_args;
            cmd_args.sess = sess;
            cmd_args.arg_num = operation->arg_num;
            cmd_args.key = key;
            cmd_args.finished = finished;
            memcpy(cmd_args.args, operation->args, sizeof(operation->args));

            return cm_run_command(command, &cmd_args);
        } else if (key_mapping->type == KMT_KEYSTR) {
            size_t keystr_len = strlen(key_mapping->value.keystr);
            return ip_add_keystr_input_to_start(&sess->input_buffer,
                                                key_mapping->value.keystr,
                                                keystr_len);
        }
    }

    if (!(key[0] == '<' && key[1] != '\0') &&
        !se_command_type_excluded(sess, CMDT_BUFFER_MOD)) {
        /* Just a normal letter character so insert it into buffer */
        return bf_insert_character(sess->active_buffer, key, 1);
    } else if (strncmp(key, "<wed-", 5) == 0 && key_mapping == NULL) {
        /* An invalid operation was specified */
        for (int k = OM_ENTRY_NUM - 1; k > -1; k--) {
            map = key_map->maps[k];

            if (rt_find(map, key, strlen(key), NULL, NULL)) {
                return st_get_error(ERR_INVALID_OPERATION_KEY_STRING,
                                    "Operation \"%s\" cannot be "
                                    "used in this context", key);
            }
        }
        
        return st_get_error(ERR_INVALID_OPERATION_KEY_STRING,
                            "Invalid operation \"%s\"", key);
    }

    return STATUS_SUCCESS;
}

int cm_is_valid_operation(const Session *sess, const char *key,
                          size_t key_len, int *is_prefix)
{
    assert(!is_null_or_empty(key));
    assert(is_prefix != NULL);

    const KeyMapping *key_mapping = NULL;
    const KeyMap *key_map = &sess->key_map;
    const RadixTree *map;
    *is_prefix = 0;

    for (int k = OM_ENTRY_NUM - 1; k > -1; k--) {
        if (key_map->active_op_modes[k]) {
            map = key_map->maps[k];
            int mode_is_prefix;

            if (rt_find(map, key, key_len, (void **)&key_mapping,
                        &mode_is_prefix)) {
                break;
            }

            *is_prefix |= mode_is_prefix;
        }
    }

    if (key_mapping != NULL ||
        !(key[0] == '<' && key[1] != '\0')) {
        *is_prefix = 0;
        return 1;
    }

    return 0;
}

Status cm_do_command(Command cmd, CommandArgs *cmd_args)
{
    const CommandDefinition *cmd_def = &cm_commands[cmd]; 
    return cm_run_command(cmd_def, cmd_args);
}

static Status cm_run_command(const CommandDefinition *cmd,
                             CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    if (se_command_type_excluded(sess, cmd->command_type)) {
        return STATUS_SUCCESS; 
    }

    const CommandSignature *cmd_sig = &cmd->command_signature;

    if (!cmd_sig->is_var_args) {
        if (cmd_args->arg_num < cmd_sig->arg_num) {
            return st_get_error(ERR_INVALID_COMMAND_ARG_NUM,
                                "Command expects %zu argument(s) "
                                "but was invoked with %zu",
                                cmd_sig->arg_num, cmd_args->arg_num);
        }

        for (size_t k = 0; k < cmd_sig->arg_num; k++) {
            if (cmd_args->args[k].type != cmd_sig->arg_types[k]) {
                return
                    st_get_error(ERR_INVALID_COMMAND_ARG_TYPE,
                                 "Command expects argument number %zu to "
                                 "have type %s but provided argument has "
                                 "type %s", k + 1,
                                 va_value_type_string(cmd_sig->arg_types[k]),
                                 va_value_type_string(cmd_args->args[k].type));
            }
        }
    }

    return cmd->command_handler(cmd_args);
}

int cm_get_command(const char *function_name, Command *cmd)
{
    if (is_null_or_empty(function_name) || cmd == NULL) {
        return 0;
    }

    static const size_t cmd_num = ARRAY_SIZE(cm_commands, CommandDefinition);

    /* TODO Load function names into a hash map for efficient lookup */

    for (size_t k = 0; k < cmd_num; k++) {
        if (cm_commands[k].function_name != NULL &&
            strcmp(function_name, cm_commands[k].function_name) == 0) {
            *cmd = k;
            return 1;
        }
    }

    return 0;
}

static Status cm_nop(const CommandArgs *cmd_args)
{
    (void)cmd_args;
    return STATUS_SUCCESS;
}

static Status cm_bp_change_line(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_change_line(sess->active_buffer, &sess->active_buffer->pos,
                          IVAL(param), 1);
}

static Status cm_bp_change_char(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_change_char(sess->active_buffer, &sess->active_buffer->pos,
                          IVAL(param), 1);
}

static Status cm_bp_to_line_start(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_line_start(sess->active_buffer, &sess->active_buffer->pos,
                            IVAL(param) & DIRECTION_WITH_SELECT, 1);
}

static Status cm_bp_to_hard_line_start(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    Buffer *buffer = sess->active_buffer;
    BufferPos *pos = &buffer->pos;
    int is_select = IVAL(param) & DIRECTION_WITH_SELECT;

    return bf_bp_to_line_start(buffer, pos, is_select, 1);
}

static Status cm_bp_to_line_end(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_line_end(sess->active_buffer,
                          IVAL(param) & DIRECTION_WITH_SELECT);
}

static Status cm_bp_to_hard_line_end(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    Buffer *buffer = sess->active_buffer;
    BufferPos *pos = &buffer->pos;
    int is_select = IVAL(param) & DIRECTION_WITH_SELECT;

    return bf_bp_to_line_end(buffer, pos, is_select, 1);
}

static Status cm_bp_to_next_word(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_next_word(sess->active_buffer,
                           IVAL(param) & DIRECTION_WITH_SELECT);
}

static Status cm_bp_to_prev_word(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_prev_word(sess->active_buffer,
                           IVAL(param) & DIRECTION_WITH_SELECT);
}

static Status cm_bp_to_buffer_start(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_buffer_start(sess->active_buffer,
                              IVAL(param) & DIRECTION_WITH_SELECT);
}

static Status cm_bp_to_buffer_end(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_buffer_end(sess->active_buffer,
                            IVAL(param) & DIRECTION_WITH_SELECT);
}

static Status cm_bp_change_page(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_change_page(sess->active_buffer, IVAL(param));
}

static Status cm_bp_to_next_paragraph(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_next_paragraph(sess->active_buffer, IVAL(param));
}

static Status cm_bp_to_prev_paragraph(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_to_prev_paragraph(sess->active_buffer, IVAL(param));
}

static Status cm_bp_goto_matching_bracket(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_jump_to_matching_bracket(sess->active_buffer);
}

static Status cm_buffer_insert_char(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];
    return bf_insert_character(sess->active_buffer, SVAL(param), 1);
}

static Status cm_buffer_delete_char(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_delete_character(sess->active_buffer);
}

static Status cm_buffer_backspace(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    if (!bf_selection_started(sess->active_buffer)) {
        if (bp_at_buffer_start(&sess->active_buffer->pos)) {
            return STATUS_SUCCESS;
        }

        Status status = bf_change_char(sess->active_buffer,
                                       &sess->active_buffer->pos,
                                       DIRECTION_LEFT, 1);
        RETURN_IF_FAIL(status);
    }

    return bf_delete_character(sess->active_buffer);
}

static Status cm_buffer_delete_word(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_delete_word(sess->active_buffer);
}

static Status cm_buffer_delete_prev_word(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_delete_prev_word(sess->active_buffer);
}

static Status cm_buffer_insert_line(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_insert_character(sess->active_buffer, "\n", 1);
}

static Status cm_buffer_select_all_text(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return bf_select_all_text(sess->active_buffer);
}

static Status cm_buffer_copy_selected_text(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return cl_copy(&sess->clipboard, sess->active_buffer);
}

static Status cm_buffer_cut_selected_text(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return cl_cut(&sess->clipboard, sess->active_buffer);
}

static Status cm_buffer_paste_text(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    return cl_paste(&sess->clipboard, sess->active_buffer);
}

static Status cm_buffer_undo(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Buffer *buffer = sess->active_buffer;
    return bc_undo(&buffer->changes, buffer);
}

static Status cm_buffer_redo(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Buffer *buffer = sess->active_buffer;
    return bc_redo(&buffer->changes, buffer);
}

static Status cm_buffer_vert_move_lines(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    Buffer *buffer = sess->active_buffer;
    return bf_vert_move_lines(buffer, IVAL(param));
}

static Status cm_buffer_duplicate_selection(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Buffer *buffer = sess->active_buffer;
    return bf_duplicate_selection(buffer);
}

static Status cm_buffer_join_lines(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Buffer *buffer = sess->active_buffer;
    const char *sep = " ";
    return bf_join_lines(buffer, sep, strlen(sep));
}

static Status cm_buffer_indent(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    Buffer *buffer = sess->active_buffer;
    Range range;

    /* Only indent if selected text spans more than one line */
    if (bf_get_range(buffer, &range) &&
        range.end.line_no != range.start.line_no) {
        return bf_indent(buffer, IVAL(param));
    }

    return bf_insert_character(sess->active_buffer, "\t", 1);
}

static Status cm_buffer_save_file(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Buffer *buffer = sess->active_buffer;
    Status status = STATUS_SUCCESS;
    int file_path_exists = fi_has_file_path(&buffer->file_info);
    int file_exists_on_disk = fi_file_exists(&buffer->file_info);
    char *file_path;

    if (!file_path_exists) {
        RETURN_IF_FAIL(cm_save_file_prompt(sess, &file_path));

        if (pr_prompt_cancelled(sess->prompt)) {
            return STATUS_SUCCESS;
        }
    } else if (file_exists_on_disk) {
        file_path = buffer->file_info.abs_path;
    } else {
        file_path = buffer->file_info.rel_path;
    }

    status = bf_write_file(buffer, file_path);
    
    if (!STATUS_IS_SUCCESS(status)) {
        if (!file_path_exists) {
            free(file_path);
        }

        return status;
    }

    if (!file_path_exists || !file_exists_on_disk) {
        /* Now that file exists on disk, initalise FileInfo for it */
        FileInfo tmp = buffer->file_info;
        status = fi_init(&buffer->file_info, file_path);
        fi_free(&tmp);

        if (!file_path_exists) {
            free(file_path);
        }

        RETURN_IF_FAIL(status);

        se_determine_filetypes_if_unset(sess, buffer);
    } else {
        fi_refresh_file_attributes(&buffer->file_info);
    }

    char msg[MAX_MSG_SIZE];
    snprintf(msg, MAX_MSG_SIZE, "Save successful: %zu lines, %zu bytes written",
                                bf_lines(buffer), bf_length(buffer));
    se_add_msg(sess, msg);

    return status;
}

static Status cm_buffer_save_as(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    Buffer *buffer = sess->active_buffer;
    char *file_path;

    RETURN_IF_FAIL(cm_save_file_prompt(sess, &file_path));

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    FileInfo orig_file_info = buffer->file_info;
    Status status = fi_init(&buffer->file_info, file_path);
    free(file_path);

    if (!STATUS_IS_SUCCESS(status)) {
        buffer->file_info = orig_file_info;
        return status;
    }

    fi_free(&orig_file_info);

    return cm_buffer_save_file(cmd_args);
}

static Status cm_save_file_prompt(Session *sess, char **file_path_ptr)
{
    cm_cmd_input_prompt(sess, PT_SAVE_FILE, "Save As:", NULL, 0);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *file_path = pr_get_prompt_content(sess->prompt);

    if (file_path == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    } else if (*file_path == '\0') {
        Status status = st_get_error(ERR_INVALID_FILE_PATH,
                                     "Invalid file path \"%s\"", file_path);
        free(file_path);
        return status;
    } 

    char *processed_path = fi_process_path(file_path);
    free(file_path);

    if (processed_path == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY,
                            "Out of memory - "
                            "Unable to process input");
    }

    *file_path_ptr = processed_path;

    return STATUS_SUCCESS;
}

static void cm_generate_find_prompt(const BufferSearch *search,
                                    char prompt_text[MAX_CMD_PROMPT_LENGTH])
{
    const char *type = "";

    if (search->search_type == BST_REGEX) {
        type = " (regex)";
    }

    const char *direction = "";

    if (!search->opt.forward) {
        direction = " (backwards)";
    }

    const char *case_sensitive = "";

    if (!search->opt.case_insensitive) {
        case_sensitive = " (case sensitive)";
    }

    snprintf(prompt_text, MAX_CMD_PROMPT_LENGTH, "Find%s%s%s:", type,
             direction, case_sensitive);
}

static Status cm_prepare_search(Session *sess, const BufferPos *start_pos)
{
    Buffer *buffer = sess->active_buffer;

    char prompt_text[MAX_CMD_PROMPT_LENGTH];
    cm_generate_find_prompt(&buffer->search, prompt_text);

    cm_cmd_input_prompt(sess, PT_FIND, prompt_text, sess->search_history, 1);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *pattern = pr_get_prompt_content(sess->prompt);

    if (pattern == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    } else if (*pattern == '\0') {
        free(pattern);
        return STATUS_SUCCESS;
    } 

    Status status = se_add_search_to_history(sess, pattern);

    if (!STATUS_IS_SUCCESS(status)) {
        free(pattern);    
        return status;
    }

    size_t pattern_len = strlen(pattern);

    if (buffer->search.search_type == BST_TEXT) {
        char *processed_pattern = su_process_string(
                                      pattern, pattern_len,
                                      buffer->file_format == FF_WINDOWS,
                                      &pattern_len);

        free(pattern);

        if (processed_pattern == NULL) {
            return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                "Unable to process input");
        } else {
            pattern = processed_pattern;
        }
    }

    status = bs_reinit(&buffer->search, start_pos, pattern, pattern_len);

    free(pattern);

    return status;
}

static Status cm_buffer_find(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    RETURN_IF_FAIL(cm_prepare_search(sess, NULL));

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    return cm_buffer_find_next(cmd_args);
}

static Status cm_buffer_find_next(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    Buffer *buffer = sess->active_buffer;

    if (buffer->search.opt.pattern == NULL) {
        return STATUS_SUCCESS;
    }

    int find_prev = IVAL(param);

    if (find_prev) {
        /* Flip the search direction */
        buffer->search.opt.forward ^= 1;
    }

    int found_match;

    Status status = bs_find_next(&buffer->search, &buffer->pos, &found_match);

    if (STATUS_IS_SUCCESS(status)) {
        if (found_match) {
            if ((buffer->search.opt.forward && 
                 bp_compare(&buffer->search.last_match_pos,
                            &buffer->pos) == -1) ||
                (!buffer->search.opt.forward &&
                 bp_compare(&buffer->search.last_match_pos,
                            &buffer->pos) == 1)) {
                /* Search wrapped past the start or end of the buffer */
                se_add_msg(sess, "Search wrapped");
            }

            status = bf_set_bp(buffer, &buffer->search.last_match_pos);
        } else {
            char msg[MAX_MSG_SIZE];
            const char *pattern = buffer->search.opt.pattern;

            if (list_size(sess->search_history) > 0) {
                pattern = list_get_last(sess->search_history);
            }

            snprintf(msg, MAX_MSG_SIZE, "Unable to find pattern: \"%s\"",
                     pattern);
            se_add_msg(sess, msg);
        }
    }

    if (find_prev) {
        /* Set the direction back again */
        buffer->search.opt.forward ^= 1;
    }

    return status;
}

static Status cm_buffer_toggle_search_direction(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    if (!se_prompt_active(sess) || 
        pr_get_prompt_type(sess->prompt) != PT_FIND) {
        return STATUS_SUCCESS;
    }

    Buffer *buffer = sess->active_buffer->next;
    buffer->search.opt.forward ^= 1;

    char prompt_text[MAX_CMD_PROMPT_LENGTH];
    cm_generate_find_prompt(&buffer->search, prompt_text);

    return pr_set_prompt_text(sess->prompt, prompt_text);
}

static Status cm_buffer_toggle_search_type(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    if (!se_prompt_active(sess) || 
        pr_get_prompt_type(sess->prompt) != PT_FIND) {
        return STATUS_SUCCESS;
    }

    Buffer *buffer = sess->active_buffer->next;

    if (buffer->search.search_type == BST_TEXT) {
        buffer->search.search_type = BST_REGEX;
    } else {
        buffer->search.search_type = BST_TEXT;
    }

    char prompt_text[MAX_CMD_PROMPT_LENGTH];
    cm_generate_find_prompt(&buffer->search, prompt_text);

    return pr_set_prompt_text(sess->prompt, prompt_text);
}

static Status cm_buffer_toggle_search_case(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    if (!se_prompt_active(sess) || 
        pr_get_prompt_type(sess->prompt) != PT_FIND) {
        return STATUS_SUCCESS;
    }

    Buffer *buffer = sess->active_buffer->next;
    buffer->search.opt.case_insensitive ^= 1;

    char prompt_text[MAX_CMD_PROMPT_LENGTH];
    cm_generate_find_prompt(&buffer->search, prompt_text);

    return pr_set_prompt_text(sess->prompt, prompt_text);
}

static Status cm_prepare_replace(Session *sess, char **rep_text_ptr,
                                 size_t *rep_length)
{
    cm_cmd_input_prompt(sess, PT_REPLACE, "Replace With:",
                        sess->replace_history, 1);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *rep_text = pr_get_prompt_content(sess->prompt);

    if (rep_text == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    } 

    *rep_length = strlen(rep_text);
    Buffer *buffer = sess->active_buffer;

    Status status = se_add_replace_to_history(sess, rep_text);

    if (!STATUS_IS_SUCCESS(status)) {
        free(rep_text);    
        return status;
    }

    RETURN_IF_FAIL(rp_replace_init(&buffer->search, rep_text, *rep_length,
                                   buffer->file_format == FF_WINDOWS));

    char *processed_rep_text = su_process_string(rep_text, *rep_length,
                                            buffer->file_format == FF_WINDOWS,
                                            rep_length);
    free(rep_text);

    if (processed_rep_text == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    }

    *rep_text_ptr = processed_rep_text;

    return status;
}

static Status cm_buffer_replace(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    Buffer *buffer = sess->active_buffer;
    RETURN_IF_FAIL(cm_prepare_search(sess, NULL));

    if (pr_prompt_cancelled(sess->prompt) ||
        buffer->search.opt.pattern == NULL) {
        return STATUS_SUCCESS;
    }

    char *rep_text = NULL;
    size_t rep_length = 0;

    Status status = cm_prepare_replace(sess, &rep_text, &rep_length);

    if (pr_prompt_cancelled(sess->prompt) || !STATUS_IS_SUCCESS(status)) {
        return status;
    }

    int found_match;
    QuestionRespose response = QR_NONE;
    BufferSearch *search = &buffer->search;
    int direction = search->opt.forward;
    size_t match_num = 0;
    size_t replace_num = 0;
    search->advance_from_last_match = (rep_length > 0);

    do {
        found_match = 0;
        status = bs_find_next(search, &buffer->pos, &found_match);

        if (found_match) {
            match_num++;
            status = bf_set_bp(buffer, &search->last_match_pos);

            if (!STATUS_IS_SUCCESS(status)) {
                break;
            }

            if (response != QR_ALL) {
                /* Select match */
                bf_select_continue(buffer);
                bp_advance_to_offset(&buffer->select_start,
                                     buffer->pos.offset + 
                                     bs_match_length(search));
                sess->ui->update(sess->ui);

                response = cm_question_prompt(sess, PT_REPLACE,
                                              "Replace (Yes|no|all):",
                                              QR_YES | QR_NO | QR_ALL, QR_YES);

                if (response == QR_ALL) {
                    /* All replacements can be undone and redone in one go */
                    status = bc_start_grouped_changes(&buffer->changes);

                    if (!STATUS_IS_SUCCESS(status)) {
                        break;
                    }

                    BufferPos buffer_start = buffer->pos;
                    bp_to_buffer_start(&buffer_start);
                    status = bf_set_bp(buffer, &buffer_start);

                    if (!STATUS_IS_SUCCESS(status)) {
                        break;
                    }

                    bs_reset(&buffer->search, &buffer_start);
                    search->advance_from_last_match = (rep_length > 0);
                    buffer->search.opt.forward = 1;
                    continue;
                }
            }

            if (response == QR_ERROR) {
                status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                      "Unable to process input");
                break;
            } else if (response == QR_CANCEL) {
                break;
            } else if (response == QR_NO) {
                if (!search->advance_from_last_match &&
                    search->opt.forward) {
                    bp_next_char(&buffer->pos);
                }
            } else if (response == QR_YES || response == QR_ALL) {
                status = rp_replace_current_match(buffer, rep_text, rep_length);

                if (!STATUS_IS_SUCCESS(status)) {
                    break;
                }

                replace_num++;
            }
        }
    } while (STATUS_IS_SUCCESS(status) && found_match);

    bf_select_reset(buffer);
    search->opt.forward = direction;

    if (bc_grouped_changes_started(&buffer->changes)) {
        bc_end_grouped_changes(&buffer->changes);
    }

    free(rep_text);

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    char msg[MAX_MSG_SIZE];

    if (match_num == 0) {
        const char *pattern = search->opt.pattern;

        if (list_size(sess->search_history) > 0) {
            pattern = list_get_last(sess->search_history);
        }

        snprintf(msg, MAX_MSG_SIZE, "Unable to find pattern \"%s\"",
                 pattern);
    } else if (replace_num == 0) {
        snprintf(msg, MAX_MSG_SIZE, "No occurrences replaced");
    } else {
        snprintf(msg, MAX_MSG_SIZE, "%zu occurrences replaced", replace_num);
    }

    se_add_msg(sess, msg);

    return status;
}

static Status cm_buffer_goto_line(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    Regex line_no_regex = { .regex_pattern = "[0-9]+", .modifiers = 0 };
    Buffer *prompt_buffer = pr_get_prompt_buffer(sess->prompt);
    bf_set_mask(prompt_buffer, &line_no_regex);

    cm_cmd_input_prompt(sess, PT_GOTO, "Line:", sess->lineno_history, 0);

    bf_remove_mask(prompt_buffer);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *input = pr_get_prompt_content(sess->prompt);

    if (input == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - "
                            "Unable to process input");
    }

    Status status = se_add_lineno_to_history(sess, input);

    if (STATUS_IS_SUCCESS(status) && *input != '\0') {
        errno = 0;

        size_t line_no = strtoull(input, NULL, 10);

        if (errno) {
            status = st_get_error(ERR_INVALID_LINE_NO,
                                  "Invalid line number \"%s\"",
                                  input);
        } else {
            status = bf_goto_line(sess->active_buffer, line_no);
        }
    }

    free(input);

    return status;
}

static Status cm_session_open_file(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    cm_cmd_input_prompt(sess, PT_OPEN_FILE, "Open:", NULL, 0);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    Status status;
    int buffer_index;

    char *input = pr_get_prompt_content(sess->prompt);

    if (input == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    } else if (*input == '\0') {
        status = st_get_error(ERR_INVALID_FILE_PATH,
                              "Invalid file path \"%s\"", input);
    } else {
        status = se_get_buffer_index_by_path(sess, input, &buffer_index);

        /* Can't find existing buffer with this path so add new buffer */
        if (STATUS_IS_SUCCESS(status) && buffer_index == -1) {
            status = se_add_new_buffer(sess, input, 0); 

            /* TODO Need a nicer way to get the index
             * of a buffer thats just been added */
            if (STATUS_IS_SUCCESS(status)) {
                buffer_index = sess->buffer_num - 1;
            }
        }
    }

    free(input);
    RETURN_IF_FAIL(status);

    se_set_active_buffer(sess, buffer_index);

    return STATUS_SUCCESS;
}

static Status cm_session_add_empty_buffer(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    RETURN_IF_FAIL(se_add_new_empty_buffer(sess));
    se_set_active_buffer(sess, sess->buffer_num - 1);

    return STATUS_SUCCESS;
}

static Status cm_session_change_tab(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    if (sess->buffer_num < 2) {
        return STATUS_SUCCESS;
    }

    size_t new_active_buffer_index;

    if (IVAL(param) == DIRECTION_RIGHT) {
        new_active_buffer_index =
            (sess->active_buffer_index + 1) % sess->buffer_num;
    } else {
        if (sess->active_buffer_index == 0) {
            new_active_buffer_index = sess->buffer_num - 1; 
        } else {
            new_active_buffer_index = sess->active_buffer_index - 1;
        }
    }

    se_set_active_buffer(sess, new_active_buffer_index);

    return STATUS_SUCCESS;
}

static Status cm_session_save_all(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    
    size_t start_buffer_index;
    
    se_get_buffer_index(sess, sess->active_buffer, &start_buffer_index);
    
    Status status = STATUS_SUCCESS;
    Buffer *buffer = sess->buffers;
    size_t buffer_save_num = 0;
    size_t buffer_index = 0;
    /* We don't want a separate message for each buffer that was saved */
    int re_enable_msgs = se_disable_msgs(sess);

    while (buffer != NULL) {
        if (bf_is_dirty(buffer)) {
            se_set_active_buffer(sess, buffer_index);
            sess->ui->update(sess->ui);
            
            status = cm_buffer_save_file(cmd_args);
            
            if (!STATUS_IS_SUCCESS(status)) {
                break;
            } else if (!pr_prompt_cancelled(sess->prompt)) {
                buffer_save_num++;
            }
        }
        
        buffer = buffer->next;
        buffer_index++;
    }
    
    se_set_active_buffer(sess, start_buffer_index);
    
    /* Only need to re-enable if enabled in the first place */
    if (re_enable_msgs) {
        se_enable_msgs(sess);
    }
    
    if (STATUS_IS_SUCCESS(status) &&
        buffer_save_num > 0) {
        char msg[MAX_MSG_SIZE];
        snprintf(msg, MAX_MSG_SIZE, "Save successful: "
                 "%zu buffers saved", buffer_save_num);
        se_add_msg(sess, msg);
    }

    return status;
}

static Status cm_session_close_buffer(const CommandArgs *cmd_args)
{
    assert(cmd_args->arg_num == 1);
    Session *sess = cmd_args->sess;
    Value param = cmd_args->args[0];

    int allow_no_buffers = IVAL(param);
    Buffer *buffer = sess->active_buffer;
    CommandArgs close_args = *cmd_args;
    close_args.args[0] = INT_VAL(0);

    if (bf_is_dirty(buffer)) {
        char prompt_text[50];
        char *fmt = "Save changes to %.*s (Y/n)?";
        snprintf(prompt_text, sizeof(prompt_text), fmt, 
                 sizeof(prompt_text) - strlen(fmt) + 3,
                 buffer->file_info.file_name);

        QuestionRespose response = cm_question_prompt(sess, PT_SAVE_FILE,
                                                      prompt_text,
                                                      QR_YES | QR_NO, QR_YES);

        if (response == QR_ERROR) {
            return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                "Unable to process input");
        } else if (response == QR_CANCEL) {
            return STATUS_SUCCESS;
        } else if (response == QR_YES) {
            RETURN_IF_FAIL(cm_buffer_save_file(&close_args));
        }

        if (pr_prompt_cancelled(sess->prompt)) {
            return STATUS_SUCCESS;
        }
    }

    se_remove_buffer(sess, buffer);

    /* We always want at least one buffer to exist unless we're exiting wed */
    if (sess->buffer_num == 0 && !allow_no_buffers) {
        return cm_session_add_empty_buffer(&close_args); 
    }

    return STATUS_SUCCESS;
}

static Status cm_session_run_command(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    cm_cmd_input_prompt(sess, PT_COMMAND, "Command:", sess->command_history, 0);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *input = pr_get_prompt_content(sess->prompt);

    if (input == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    }

    Status status = se_add_cmd_to_history(sess, input);

    if (STATUS_IS_SUCCESS(status) && *input != '\0') {
        status = cp_parse_config_string(sess, CL_BUFFER, input);
    }

    free(input);

    return status;
}

static Status cm_previous_prompt_entry(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    return pr_previous_entry(sess->prompt);
}

static Status cm_next_prompt_entry(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    return pr_next_entry(sess->prompt);
}

static Status cm_prompt_input_finished(const CommandArgs *cmd_args)
{
    Status status = STATUS_SUCCESS;
    Session *sess = cmd_args->sess;

    if (pr_get_prompt_type(sess->prompt) == PT_COMMAND &&
        bf_length(sess->active_buffer) > 0) {
        BufferPos *pos = &sess->active_buffer->pos;
        bp_to_buffer_end(pos);

        BufferPos last_char_pos = *pos;
        bp_prev_char(&last_char_pos);
        char last_char = bp_get_char(&last_char_pos);

        if (last_char != ';') {
            status = bf_insert_character(sess->active_buffer, ";", 1);
        }
    }

    *cmd_args->finished = 1;

    return status;
}

static Status cm_session_change_buffer(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    cm_cmd_input_prompt(sess, PT_BUFFER, "Buffer:", sess->buffer_history, 0);

    if (pr_prompt_cancelled(sess->prompt)) {
        return STATUS_SUCCESS;
    }

    char *input = pr_get_prompt_content(sess->prompt);

    if (input == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to process input");
    }

    Status status = se_add_buffer_to_history(sess, input);
    int empty = (*input == '\0');
    const Buffer *buffer;

    if (STATUS_IS_SUCCESS(status) && !empty) {
        /* Try and match user input to buffer */
        status = cm_determine_buffer(sess, input, &buffer);
    }

    free(input);

    if (!STATUS_IS_SUCCESS(status) || empty) {
        return status;
    }

    size_t buffer_index;

    if (!se_get_buffer_index(sess, buffer, &buffer_index)) {
        assert(!"Buffer has no valid buffer index");
    }

    se_set_active_buffer(sess, buffer_index);

    return status;
}

static Status cm_determine_buffer(Session *sess, const char *input, 
                                  const Buffer **buffer_ptr)
{
    Prompt *prompt = sess->prompt;
    size_t input_len = strlen(input);

    RegexInstance regex;
    RegexResult regex_result; 
    Regex numeric_regex = { 
        .regex_pattern = "^\\s*([0-9]+)\\s*$",
        .modifiers = 0
    };

    RETURN_IF_FAIL(ru_compile(&regex, &numeric_regex));
    Status status = ru_exec(&regex_result, &regex, input, input_len, 0);
    ru_free_instance(&regex);
    RETURN_IF_FAIL(status);

    /* First group is entire match.
     * Second group is the one we specified in the above regex.
     * If PCRE captured two groups then our regex matched.
     * See man pcreapi for more details */
    int is_numeric = (regex_result.return_code == 2);

    if (is_numeric) {
        /* User has entered a buffer id */
        char *group_str;
        RETURN_IF_FAIL(ru_get_group(&regex_result, input, input_len,
                                    1, &group_str));
        errno = 0;
        size_t buffer_index = strtoull(input, NULL, 10);
        free(group_str);

        if (errno == 0 &&
            /* Buffer indexes displayed to users start from 1
             * so we need to subtract 1 from the value
             * they entered */
            buffer_index-- > 0 &&
            se_is_valid_buffer_index(sess, buffer_index)) {
            *buffer_ptr = se_get_buffer(sess, buffer_index);
            return STATUS_SUCCESS;  
        }
    }

    RETURN_IF_FAIL(pc_run_prompt_completer(sess, prompt, 0));
    size_t suggestion_num = list_size(prompt->suggestions);

    /* User input is added to prompt->suggestions
     * (so that it can be cycled back to) such that 
     * if suggestions were found suggestion_num
     * should be at least 2 */
    if (suggestion_num < 2) {
        return st_get_error(ERR_NO_BUFFERS_MATCH,
                            "No buffers match \"%s\"",
                            input);
    }

    const PromptSuggestion *suggestion = list_get_first(prompt->suggestions);

    if (!(suggestion->rank == SR_EXACT_MATCH ||
          suggestion_num == 2)) {
        return st_get_error(ERR_MULTIPLE_BUFFERS_MATCH, 
                            "Multiple (%zu) buffers match \"%s\"",
                            suggestion_num - 1, input);
    }

    *buffer_ptr = suggestion->data;
    return STATUS_SUCCESS;
}

static Status cm_suspend(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    sess->ui->suspend(sess->ui);
    kill(0, SIGTSTP);

    return STATUS_SUCCESS;
}

static Status cm_session_end(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;

    CommandArgs close_args = *cmd_args;
    close_args.arg_num = 1;
    close_args.args[0] = INT_VAL(1);

    pr_prompt_set_cancelled(sess->prompt, 0);

    while (sess->buffer_num > 0) {
        RETURN_IF_FAIL(cm_session_close_buffer(&close_args));

        if (pr_prompt_cancelled(sess->prompt)) {
            return STATUS_SUCCESS;
        }
    }  

    *cmd_args->finished = 1;
    se_set_session_finished(sess);

    return STATUS_SUCCESS;
}

static QuestionRespose cm_question_prompt(Session *sess, PromptType prompt_type,
                                          const char *question,
                                          QuestionRespose allowed_answers,
                                          QuestionRespose default_answer)
{
    QuestionRespose response = QR_NONE;

    do {
        cm_cmd_input_prompt(sess, prompt_type, question, NULL, 0);

        if (pr_prompt_cancelled(sess->prompt)) {
            return QR_CANCEL;
        }

        char *input = pr_get_prompt_content(sess->prompt);

        if (input == NULL) {
            return QR_ERROR;
        } else if ((allowed_answers & default_answer) && *input == '\0') {
            response = default_answer;
        } else if ((allowed_answers & QR_YES) &&
                   (*input == 'y' || *input == 'Y')) {
            response = QR_YES;
        } else if ((allowed_answers & QR_NO) &&
                   (*input == 'n' || *input == 'N')) {
            response = QR_NO;
        } else if ((allowed_answers & QR_ALL) &&
                   (*input == 'a' || *input == 'A')) {
            response = QR_ALL;
        }

        free(input);
    } while (response == QR_NONE);

    return response;
}

static Status cm_cmd_input_prompt(Session *sess, PromptType prompt_type, 
                                  const char *prompt_text, List *history,
                                  int show_last_cmd)
{
    RETURN_IF_FAIL(se_make_prompt_active(sess, prompt_type,
                                         prompt_text, history,
                                         pc_has_prompt_completer(prompt_type),
                                         show_last_cmd));

    CommandType disabled_cmd_types = CMDT_CMD_INPUT | CMDT_SESS_MOD;
    se_exclude_command_type(sess, disabled_cmd_types);

    sess->ui->update(sess->ui);
    /* We now start processing input for the prompt.
     * Execution blocks here until the prompt ends */
    ip_process_input(sess);
    /* The prompt has ended. We now can access the 
     * text (if any) the user entered into the prompt */

    se_enable_command_type(sess, disabled_cmd_types);
    se_end_prompt(sess);
    bf_set_is_draw_dirty(sess->active_buffer, 1);

    return STATUS_SUCCESS;
}

static Status cm_cancel_prompt(const CommandArgs *cmd_args)
{
    pr_prompt_set_cancelled(cmd_args->sess->prompt, 1);
    *cmd_args->finished = 1;

    return STATUS_SUCCESS;
}

static Status cm_run_prompt_completion(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    const char *key = cmd_args->key;

    if (!se_prompt_active(sess)) {
        return STATUS_SUCCESS;
    }

    Prompt *prompt = sess->prompt;
    Status status = STATUS_SUCCESS;
    const char *prev_key = se_get_prev_key(sess);

    int prev_key_is_completer = 
        strncmp(prev_key, "<Tab>", MAX_KEY_STR_SIZE) == 0 ||
        strncmp(prev_key, "<S-Tab>", MAX_KEY_STR_SIZE) == 0;

    if (prev_key_is_completer) {
        if (strncmp(key, "<Tab>", MAX_KEY_STR_SIZE) == 0) {
            status = pr_show_next_suggestion(prompt);
        } else if (strncmp(key, "<S-Tab>", MAX_KEY_STR_SIZE) == 0) {
            status = pr_show_previous_suggestion(prompt);
        }
    } else {
        int reverse = (strncmp(key, "<S-Tab>", MAX_KEY_STR_SIZE) == 0);
        status = pc_run_prompt_completer(sess, prompt, reverse);
    }

    if (STATUS_IS_SUCCESS(status) &&
        pc_show_suggestion_prompt(prompt->prompt_type)) {
        pr_show_suggestion_prompt(prompt);
    }

    return status;
}

static Status cm_session_echo(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    Status status = STATUS_SUCCESS;

    if (!se_initialised(sess)) {
        return status;
    }

    char *str_val;

    for (size_t k = 0; k < cmd_args->arg_num; k++) {
        str_val = va_to_string(cmd_args->args[k]);

        if (cf_str_to_var(str_val, NULL)) {
            ConfigLevel config_level = cp_determine_config_level(str_val,
                                                                 CL_BUFFER);

            status = cf_print_var(CE_VAL(sess, sess->active_buffer),
                                  config_level, str_val);

            if (!STATUS_IS_SUCCESS(status)) {
                break;
            }
        } else {
            se_add_msg(sess, str_val); 
        }

        free(str_val);
    }

    return status;
}

static Status cm_session_map(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    char *map_from;
    char *map_to;
    
    map_from = va_to_string(cmd_args->args[0]);
    map_to = va_to_string(cmd_args->args[1]);

    if (is_null_or_empty(map_from) || is_null_or_empty(map_to)) {
        free(map_from);
        free(map_to);
        return st_get_error(ERR_INVALID_KEY_MAPPING,
                            "Key mappings must be non-empty");
    }

    KeyMapping *key_mapping = cm_new_keystr_key_mapping(map_from, map_to);

    free(map_from);
    free(map_to);

    if (key_mapping == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to create key mapping");
    }

    KeyMap *key_map = &sess->key_map;
    RadixTree *map = key_map->maps[OM_USER];
    KeyMapping *existing_key_mapping = NULL;
    size_t key_len = strlen(key_mapping->key);

    if (rt_find(map, key_mapping->key, key_len,
                (void **)&existing_key_mapping, NULL)) {
        rt_delete(map, key_mapping->key, key_len);
        cm_free_key_mapping(existing_key_mapping);
    }

    if (!rt_insert(map, key_mapping->key, key_len, key_mapping)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to create key mapping");
    }

    key_map->active_op_modes[OM_USER] = 1;

    char msg[MAX_MSG_SIZE];
    snprintf(msg, MAX_MSG_SIZE, "Mapped %s to %s", key_mapping->key,
             key_mapping->value.keystr);
    se_add_msg(sess, msg);

    return STATUS_SUCCESS;
}

static Status cm_session_unmap(const CommandArgs *cmd_args)
{
    Session *sess = cmd_args->sess;
    Status status = STATUS_SUCCESS;
    char *map_from = va_to_string(cmd_args->args[0]);

    if (is_null_or_empty(map_from)) {
        free(map_from);
        return st_get_error(ERR_INVALID_KEY_MAPPING,
                            "Key mappings must be non-empty");
    }

    KeyMap *key_map = &sess->key_map;
    RadixTree *map = key_map->maps[OM_USER];
    KeyMapping *existing_key_mapping = NULL;
    size_t map_from_len = strlen(map_from);

    if (!rt_find(map, map_from, map_from_len,
                 (void **)&existing_key_mapping, NULL)) {
        status = st_get_error(ERR_INVALID_KEY_MAPPING,
                              "No mappping exists for %s", map_from);
        free(map_from);
        return status;
    }

    rt_delete(map, map_from, map_from_len);
    cm_free_key_mapping(existing_key_mapping);

    if (rt_entries(map) == 0) {
        key_map->active_op_modes[OM_USER] = 0;
    }

    char msg[MAX_MSG_SIZE];
    snprintf(msg, MAX_MSG_SIZE, "Unmapped %s", map_from);
    se_add_msg(sess, msg);

    free(map_from);

    return status;
}
