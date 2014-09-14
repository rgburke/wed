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

#ifndef WED_DISPLAY_H
#define WED_DISPLAY_H

#include "session.h"
#include "buffer.h"

#define MENU_COLOR_PAIR 1
#define TAB_COLOR_PAIR 2
#define STATUS_COLOR_PAIR 3

/* TODO Is this needed anymore? */
typedef enum {
    WIN_MENU,
    WIN_TEXT,
    WIN_STATUS 
} Window;

/* A screen representation of a BufferPos in terms of a line and column number.
 * This includes counting wrapped lines as lines in their own right. */
typedef struct {
    size_t line_no;
    size_t col_no;
} Point;

void init_display(void);
void end_display(void);
void refresh_display(Session *);
void draw_menu(Session *sess);
void draw_status(Session *);
void draw_text(Session *, int);
void move_cursor(Window, int, int);
void update_display(Session *);
size_t byte_screen_length(char, size_t);
size_t char_byte_length(char);
size_t editor_screen_width(void);
size_t editor_screen_height(void);
size_t line_screen_length(Line *, size_t, size_t);
size_t line_screen_height(Line *);
size_t line_pos_screen_height(BufferPos);
size_t line_offset_screen_height(Line *, size_t, size_t);
size_t screen_height_from_screen_length(size_t);

#endif
