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
#include "status.h"
#include "command.h"
#include "session.h"
#include "display.h"
#include "buffer.h"
#include "variable.h"

static Status bufferpos_change_line(Session *, Value, int *);
static Status bufferpos_change_char(Session *, Value, int *); 
static Status quit_wed(Session *, Value, int *);

static const Command commands[] = {
    { KEY_UP   , bufferpos_change_line, INT_VAL(-1) },
    { KEY_DOWN , bufferpos_change_line, INT_VAL(1)  },
    { KEY_RIGHT, bufferpos_change_char, INT_VAL(1)  },
    { KEY_LEFT , bufferpos_change_char, INT_VAL(-1) },
    { KEY_F(2) , quit_wed             , INT_VAL(0)  }
};

Status do_command(Session *sess, int command, int *quit)
{
    size_t command_num = sizeof(commands) / sizeof(Command);

    for (size_t k = 0; k < command_num; k++) {
        if (commands[k].code == command) {
            return commands[k].func(sess, commands[k].param, quit);
        }
    }

    /* TODO Update this with an Status with an error */
    return STATUS_FAIL;
}

static Status bufferpos_change_line(Session *sess, Value param, int *quit)
{
    (void)quit;
    return pos_change_screen_line(sess->active_buffer, &sess->active_buffer->pos, param.val.ival);
}

static Status bufferpos_change_char(Session *sess, Value param, int *quit)
{
    (void)quit;
    return pos_change_char(sess->active_buffer, &sess->active_buffer->pos, param.val.ival);
}

static Status quit_wed(Session *sess, Value param, int *quit)
{
    (void)sess;
    (void)&param;
    *quit = true;
    return STATUS_SUCCESS;
}

