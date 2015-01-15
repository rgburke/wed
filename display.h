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

typedef enum {
    WIN_MENU,
    WIN_TEXT,
    WIN_STATUS
} DrawWindow;

void init_display(void);
void end_display(void);
void init_all_window_info(Session *);
void init_window_info(WindowInfo *);
void update_display(Session *);
size_t screen_col_no(WindowInfo, BufferPos);
size_t line_screen_length(Buffer *, BufferPos, size_t);
size_t line_screen_height(WindowInfo, Line *);
size_t screen_height_from_screen_length(WindowInfo, size_t);

#endif
