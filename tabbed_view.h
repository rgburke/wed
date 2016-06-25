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

#ifndef WED_TABBED_VIEW_H
#define WED_TABBED_VIEW_H

#include "buffer_view.h"
#include "util.h"
#include "session.h"

/* Put a limit on the number of buffer tabs that can be drawn */
#define MAX_VISIBLE_BUFFER_TABS 30
/* Maximum width a single buffer tab can occupy */
#define MAX_BUFFER_TAB_WIDTH 30
/* The status bar is split into 2 or 3 sections depending on whether there
 * are messages to display */
#define MAX_STATUS_BAR_SECTIONS 3
/* Put a limit on the length of a status message */
#define MAX_STATUS_BAR_SECTION_WIDTH 512

/* This structure is an in memory representation of the entire display that
 * is eventually drawn to a window */
typedef struct {
    BufferView *bv; /* The active buffers display data */
    /* A list of buffer tab names to be displayed along the top of the
     * display */
    char buffer_tabs[MAX_VISIBLE_BUFFER_TABS][MAX_BUFFER_TAB_WIDTH];
    size_t first_buffer_tab_index; /* The buffer index of the first buffer
                                      tab */
    size_t buffer_tab_num; /* Number of buffer tabs to display */
    /* Status info displayed in the bottom line of the window */
    char status_bar[MAX_STATUS_BAR_SECTIONS][MAX_STATUS_BAR_SECTION_WIDTH];
    size_t rows; /* The total display rows available */
    size_t cols; /* The total display columns available */
    size_t line_no_width; /* Number of columns required to display line
                             numbers */
    size_t last_line_no_width; /* Columns required to display line numbers
                                  from the last update */
    int is_prompt_active; /* True if the prompt is active */
    const char *prompt_text; /* The prompt text to display */
    size_t prompt_text_len; /* Length of the prompt text */
} TabbedView;

void tv_init(TabbedView *, size_t rows, size_t cols);
void tv_free(TabbedView *);
Status tv_update(TabbedView *, Session *);
void tv_resize(TabbedView *, size_t rows, size_t cols);

#endif
