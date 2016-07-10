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

#ifndef WED_INPUT_H
#define WED_INPUT_H

#include "lib/libtermkey/termkey.h"
#include "gap_buffer.h"
#include "status.h"

struct Session;

typedef enum {
    IT_FD,
    IT_KEYSTR
} InputType;

typedef struct {
    InputType input_type;
    TermKey *termkey;
    const char *keystr_input;
    const char *iter;
} InputHandler;

typedef enum {
    IA_NO_INPUT_AVAILABLE_TO_READ,
    IA_INPUT_AVAILABLE_TO_READ
} InputArgument;

typedef enum {
    IR_NO_INPUT_ADDED,
    IR_INPUT_ADDED,
    IR_WAIT_FOR_MORE_INPUT,
    IR_EOF
} InputResult;

typedef struct {
    GapBuffer *buffer;
    InputArgument arg;
    InputResult result;
    size_t wait_time_nano;
} InputBuffer;

int ip_init(InputBuffer *);
void ip_free(InputBuffer *);
void ip_edit(struct Session *);
void ip_process_input(struct Session *);
Status ip_add_keystr_input_to_start(InputBuffer *, const char *keystr,
                                    size_t keystr_len);
Status ip_add_keystr_input_to_end(InputBuffer *, const char *keystr,
                                  size_t keystr_len);

#endif
