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
#include "variable.h"

typedef Status (*CommandHandler)(Session *, Value, const char *, int *);

/* Represents a command. Move up, down etc... */
typedef struct {
    const char *keystr; /* Key combo string representiation */
    CommandHandler command_handler; /* Pointer to command function */
    Value param; /* Argument passed to command function */
    CommandType cmd_type; /* What type of action does this command perform */
} Command;

int init_keymap(Session *);
Status do_command(Session *, const char *, int *);

#endif
