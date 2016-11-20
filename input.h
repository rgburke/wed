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

/* Used to inform the input buffer whether input is available to be read or
 * not */
typedef enum {
    IA_NO_INPUT_AVAILABLE_TO_READ,
    IA_INPUT_AVAILABLE_TO_READ
} InputArgument;

/* Used to signal whether data was read into the input buffer or not */
typedef enum {
    IR_NO_INPUT_ADDED,
    IR_INPUT_ADDED,
    IR_WAIT_FOR_MORE_INPUT,
    IR_EOF
} InputResult;

/* Mouse click event types supported by wed */
typedef enum {
    MCET_BUFFER, /* A position in a buffer was clicked */
    MCET_TAB /* A buffer tab was clicked */
} MouseClickEventType;

/* Data about the type of mouse click event detected */
typedef enum {
    MCT_PRESS = TERMKEY_MOUSE_PRESS,
    MCT_DRAG = TERMKEY_MOUSE_DRAG,
    MCT_RELEASE = TERMKEY_MOUSE_RELEASE
} MouseClickType;

/* The row and column clicked */
typedef struct {
    size_t row;
    size_t col;
} ClickPos;

/* Structure containing all data for a mouse event */
typedef struct {
    MouseClickEventType event_type; /* The event type */
    MouseClickType click_type; /* Click type (press, release, etc...) */
    union {
        ClickPos click_pos; /* The position in a buffer that was clicked */
        size_t buffer_index; /* The buffer that was clicked on */
    } data; /* Data relating to this event */
} MouseClickEvent;

/* Structure through which input is read in and stored to be processed */
typedef struct {
    GapBuffer *buffer; /* Store key string input */
    InputArgument arg; /* Specifies whether input is available to be read into
                          the input buffer */
    InputResult result; /* Specifies whether input was read into the input
                           buffer */
    size_t wait_time_nano; /* The amount of time that should be waited to
                              see if new input becomes available before
                              processing continues */
    MouseClickEvent last_mouse_click; /* The last mouse click event that
                                         occurred */
} InputBuffer;

int ip_init(InputBuffer *);
void ip_free(InputBuffer *);
void ip_edit(struct Session *);
void ip_process_input(struct Session *);
Status ip_add_keystr_input_to_start(InputBuffer *, const char *keystr,
                                    size_t keystr_len);
Status ip_add_keystr_input_to_end(InputBuffer *, const char *keystr,
                                  size_t keystr_len);
Status ip_add_mouse_click_event(InputBuffer *, const char *keystr,
                                size_t keystr_len, const MouseClickEvent *);
const MouseClickEvent *ip_get_last_mouse_click_event(const InputBuffer *);

#endif
