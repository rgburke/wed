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

#include <ncurses.h>
#include <string.h>
#include "status.h"
#include "command.h"
#include "session.h"
#include "display.h"
#include "buffer.h"
#include "variable.h"
#include "hashmap.h"

static Status bufferpos_change_line(Session *, Value, int *);
static Status bufferpos_change_char(Session *, Value, int *); 
static Status bufferpos_to_line_start(Session *, Value, int *);
static Status bufferpos_to_line_end(Session *, Value, int *);
static Status bufferpos_to_next_word(Session *, Value, int *);
static Status bufferpos_to_prev_word(Session *, Value, int *);
static Status bufferpos_to_buffer_start(Session *, Value, int *);
static Status bufferpos_to_buffer_end(Session *, Value, int *);
static Status bufferpos_change_page(Session *, Value, int *);
static Status buffer_insert_char(Session *, Value, int *);
static Status buffer_delete_char(Session *, Value, int *);
static Status buffer_backspace(Session *, Value, int *);
static Status buffer_insert_line(Session *, Value, int *);
static Status quit_wed(Session *, Value, int *);

static const Command commands[] = {
    { "<Up>"        , bufferpos_change_line    , INT_VAL_STRUCT(-1)   },
    { "<Down>"      , bufferpos_change_line    , INT_VAL_STRUCT(1)    },
    { "<Right>"     , bufferpos_change_char    , INT_VAL_STRUCT(1)    },
    { "<Left>"      , bufferpos_change_char    , INT_VAL_STRUCT(-1)   },
    { "<Home>"      , bufferpos_to_line_start  , INT_VAL_STRUCT(0)    },
    { "<End>"       , bufferpos_to_line_end    , INT_VAL_STRUCT(0)    },
    { "<C-Right>"   , bufferpos_to_next_word   , INT_VAL_STRUCT(0)    },
    { "<C-Left>"    , bufferpos_to_prev_word   , INT_VAL_STRUCT(0)    },
    { "<C-Home>"    , bufferpos_to_buffer_start, INT_VAL_STRUCT(0)    },
    { "<C-PageUp>"  , bufferpos_to_buffer_start, INT_VAL_STRUCT(0)    },
    { "<C-End>"     , bufferpos_to_buffer_end  , INT_VAL_STRUCT(0)    },
    { "<C-PageDown>", bufferpos_to_buffer_end  , INT_VAL_STRUCT(0)    },
    { "<PageUp>"    , bufferpos_change_page    , INT_VAL_STRUCT(-1)   },
    { "<PageDown>"  , bufferpos_change_page    , INT_VAL_STRUCT(1)    },
    { "<Space>"     , buffer_insert_char       , STR_VAL_STRUCT(" ")  },
    { "<Tab>"       , buffer_insert_char       , STR_VAL_STRUCT("\t") },
    { "<Delete>"    , buffer_delete_char       , INT_VAL_STRUCT(0)    },
    { "<Backspace>" , buffer_backspace         , INT_VAL_STRUCT(0)    },
    { "<Enter>"     , buffer_insert_line       , INT_VAL_STRUCT(0)    },
    { "<F2>"        , quit_wed                 , INT_VAL_STRUCT(0)    }
};

int init_keymap(Session *sess)
{
    size_t command_num = sizeof(commands) / sizeof(Command);

    sess->keymap = new_sized_hashmap(command_num * 2);

    if (sess->keymap == NULL) {
        return 0;
    }

    for (size_t k = 0; k < command_num; k++) {
        hashmap_set(sess->keymap, commands[k].keystr, (Command *)&commands[k]);
    }

    return 1;
}

Status do_command(Session *sess, char *command_str, int *quit)
{
    Command *command = hashmap_get(sess->keymap, command_str);

    if (command != NULL) {
        return command->func(sess, command->param, quit);
    }

    if (!(command_str[0] == '<' && command_str[1] != '\0')) {
        return insert_character(sess->active_buffer, command_str);
    }

    return STATUS_SUCCESS;
}

static Status bufferpos_change_line(Session *sess, Value param, int *quit)
{
    (void)quit;
    return pos_change_screen_line(sess->active_buffer, &sess->active_buffer->pos, param.val.ival, 1);
}

static Status bufferpos_change_char(Session *sess, Value param, int *quit)
{
    (void)quit;
    return pos_change_char(sess->active_buffer, &sess->active_buffer->pos, param.val.ival, 1);
}

static Status bufferpos_to_line_start(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_screen_line_start(sess->active_buffer);
}

static Status bufferpos_to_line_end(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_screen_line_end(sess->active_buffer);
}

static Status bufferpos_to_next_word(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_next_word(sess->active_buffer);
}

static Status bufferpos_to_prev_word(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_prev_word(sess->active_buffer);
}

static Status bufferpos_to_buffer_start(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_buffer_start(sess->active_buffer);
}

static Status bufferpos_to_buffer_end(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return pos_to_buffer_end(sess->active_buffer);
}

static Status bufferpos_change_page(Session *sess, Value param, int *quit)
{
    (void)quit;
    return pos_change_page(sess->active_buffer, param.val.ival);
}

static Status buffer_insert_char(Session *sess, Value param, int *quit)
{
    (void)quit;
    return insert_character(sess->active_buffer, param.val.sval);
}

static Status buffer_delete_char(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return delete_character(sess->active_buffer);
}

static Status buffer_backspace(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;

    Status status = pos_change_char(sess->active_buffer, &sess->active_buffer->pos, -1, 1);

    if (!is_success(status)) {
        return status;
    }

    return delete_character(sess->active_buffer);
}

static Status buffer_insert_line(Session *sess, Value param, int *quit)
{
    (void)quit;
    (void)param;
    return insert_line(sess->active_buffer);
}

static Status quit_wed(Session *sess, Value param, int *quit)
{
    (void)sess;
    (void)&param;
    *quit = true;
    return STATUS_SUCCESS;
}

