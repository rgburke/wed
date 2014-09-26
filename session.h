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

#include "buffer.h"
#include "status.h"
#include "hashmap.h"

/* Top level structure containing all state.
 * A new session is created when wed is invoked. */
typedef struct {
    Buffer *buffers;
    Buffer *active_buffer;
    ErrorQueue error_queue;
    HashMap *keymap;
} Session;

Session *new_session(void);
int init_session(Session *, char **, int);
void free_session(Session *);
int add_buffer(Session *, Buffer *);
size_t get_buffer_num(Session *);
int set_active_buffer(Session *, size_t);
int add_err(Session *, Error *);
int add_error(Session *, Status);

#endif
