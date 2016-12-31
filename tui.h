/*
 * Copyright (C) 2016 Richard Burke
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

#ifndef WED_TUI_H
#define WED_TUI_H

#include <ncurses.h>
#include "lib/libtermkey/termkey.h"
#include "ui.h"
#include "tabbed_view.h"
#include "session.h"

/* Store mouse press data to determine if a double click has taken place */
typedef struct {
    MouseClickEvent last_mouse_press; /* Last mouse press data */
    const WINDOW *click_win; /* Last mouse press window */
    struct timespec last_mouse_press_time; /* Last mouse press time */
} DoubleClickMonitor;

/* This implements the UI interface from ui.h */
typedef struct {
    UI ui; /* Extend the UI structure. TUI specific function pointers will be
              assigned to the members of this structure */
    TabbedView tv; /* The view to be drawn to the terminal window */
    Session *sess; /* Reference to the session for this UI */
    size_t rows; /* The number of rows available as determined from ncurses */
    size_t cols; /* The number of columns available as determined from
                    ncurses */
    WINDOW *menu_win; /* Used to display buffer tabs */
    WINDOW *buffer_win; /* Used to display the buffer content */
    WINDOW *status_win; /* Used to display status info and the prompt when
                           active */
    WINDOW *line_no_win; /* Used to display line numbers when active */
    WINDOW *file_explorer_win; /* Used to display the file explorer */
    mmask_t mouse_mask; /* Previous mouse settings which can be toggled
                           back to */
    TermKey *termkey; /* Use to process user input */
    DoubleClickMonitor double_click_monitor; /* Monitor mouse clicks for
                                                double click occurrences */
} TUI;

UI *ti_new(Session *);

#endif
