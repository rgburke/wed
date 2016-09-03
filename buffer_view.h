/*
 * Copyright (C) 2016 Richard Burke
 * Inspired by view.h from the vis text editor by Marc André Tanner
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

#ifndef WED_BUFFER_VIEW_H
#define WED_BUFFER_VIEW_H

#include <stddef.h>
#include "syntax.h"
#include "buffer_pos.h"
#include "undo.h"

/* The code in this file is used to generate an in memory representation of
 * a buffers display. It is ultimately passed to tui.c to be drawn */

/* The maximum character byte length a cell allows. This is large to
 * allow the possibility of including combining characters with a character */
#define CELL_TEXT_LENGTH 50

/* Attributes each Cell structure can have that influence how they're
 * displayed */
typedef enum {
    CA_NONE = 0,
    CA_CURSOR = 1, /* Cursor location */
    CA_SELECTION = 1 << 1, /* Part of a selection */
    CA_BUFFER_END = 1 << 2, /* Lines after buffer content */
    CA_ERROR = 1 << 3, /* Error message */
    CA_WRAP = 1 << 4, /* Displays a wrap character */
    CA_COLORCOLUMN = 1 << 5, /* Is on colorcolumn */
    CA_NEW_LINE = 1 << 6, /* Cell represents new line character */
    CA_LINE_END = 1 << 7, /* Empty cells after a new line */
    CA_SEARCH_MATCH = 1 << 8 /* Regions that match the current search */
} CellAttribute;

/* Structure representing each cell in a window */
typedef struct {
    char text[CELL_TEXT_LENGTH]; /* Character bytes */
    size_t text_len; /* Character length */
    size_t col_width; /* Number of columns this character requires to be
                         displayed */
    size_t offset; /* Location of this character in the buffer */
    size_t col_no; /* The computed column number of this character */
    CellAttribute attr; /* Bitmask of cell attributes */
    SyntaxToken token; /* Syntax token for this character */
} Cell;

/* Line structure used to represent screen line */
typedef struct {
    size_t line_no; /* Line number for this line, 0 when this represents a
                       wrapped line */
    Cell *cells; /* Array of cells */
} Line;

/* Cached Syntax matches. Syntax matches are expensive to generate so only
 * do so when necessary */
typedef struct {
    SyntaxMatches *syn_matches; /* Cached SyntaxMatches */
    BufferPos screen_start; /* The value of screen_start when syn_matches
                               were generated */
} SyntaxMatchCache;

/* In memory representation of buffer content as it appears on the screen
 * i.e. It is effectively a 2 dimensional array of cells with the same
 * dimensions as the buffer window that will be used to display it */
typedef struct {
    size_t rows; /* The number of lines this view contains */
    size_t cols; /* The number of cells each line contains */
    size_t rows_allocated; /* Number of lines actually allocated,
                              is always >= rows */
    size_t cols_allocated; /* Number of cells actually allocated in each line,
                              is always >= cols */
    Line *lines; /* Array of lines */
    BufferPos screen_start; /* Where this view starts from */
    size_t horizontal_scroll; /* This value is calculated on each update and
                                 is the horizontal scroll required for the
                                 buffer view to correctly display the buffer.
                                 This value is only used when linewrap=false */
    SyntaxMatchCache syn_match_cache; /* Cached syntax matches */
    BufferChangeState change_state; /* Used to track if the buffer has been
                                       modified since the last update */
    int resized; /* True when the display has been resized and a redraw is
                    required */
    size_t rows_drawn; /* The number of rows containing buffer content */
} BufferView;

struct Session;
struct Buffer;

BufferView *bv_new(size_t rows, size_t cols, const BufferPos *screen_start);
void bv_free(BufferView *);
void bv_update_view(const struct Session *, struct Buffer *);
int bv_resize(BufferView *, size_t rows, size_t cols);
size_t bv_screen_col_no(const struct Buffer *, const BufferPos *);
void bv_apply_cell_attributes(BufferView *, CellAttribute attr,
                              CellAttribute exclude_cell_attr);
void bv_free_syntax_match_cache(BufferView *);

#endif
