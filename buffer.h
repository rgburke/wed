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

#ifndef WED_BUFFER_H
#define WED_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include "shared.h"
#include "status.h"
#include "file.h"
#include "hashmap.h"
#include "encoding.h"
#include "gap_buffer.h"
#include "buffer_pos.h"
#include "search.h"
#include "undo.h"

typedef enum {
    CCLASS_WHITESPACE,
    CCLASS_PUNCTUATION,
    CCLASS_WORD
} CharacterClass;

typedef enum {
    DIRECTION_NONE,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_WITH_SELECT = 1 << 3,
} Direction;

/* Represent selected text in a buffer,
 * start is inclusive, end is exclusive */
typedef struct {
    BufferPos start;
    BufferPos end;
} Range;

typedef struct {
    char *str;
    size_t str_len;
} TextSelection;

typedef struct {
    size_t height;
    size_t width;
    size_t start_y;
    size_t start_x;
    size_t line_no_width;
    size_t horizontal_scroll;
    DrawWindow draw_window;
} WindowInfo;

typedef struct Buffer Buffer;

/* The in memory representation of a file */
struct Buffer {
    FileInfo file_info; /* stat like info */
    BufferPos pos; /* The cursor position */
    BufferPos screen_start; /* The first screen line (can start on wrapped line) to start drawing from */
    BufferPos select_start; /* Starting position of selected text */
    Buffer *next; /* Next buffer in this session */
    size_t line_col_offset; /* Global cursor line offset */
    WindowInfo win_info; /* Window dimension info */
    HashMap *config; /* Stores config variables */
    CharacterEncodingType encoding_type;
    CharacterEncodingFunctions cef;
    int is_dirty;
    GapBuffer *data;
    BufferSearch search;
    BufferChanges changes;
};

Buffer *bf_new(const FileInfo *);
Buffer *bf_new_empty(const char *);
void bf_free(Buffer *);
Status bf_clear(Buffer *);
Status bf_load_file(Buffer *);
Status bf_write_file(Buffer *, const char *);
char *bf_to_string(const Buffer *);
char *bf_join_lines(const Buffer *, const char *);
int bf_is_empty(const Buffer *);
size_t bf_lines(const Buffer *);
size_t bf_length(const Buffer *);
int bf_get_range(const Buffer *, Range *);
int bf_bp_in_range(const Range *, const BufferPos *);
CharacterClass bf_character_class(const BufferPos *);
int bf_bp_at_screen_line_start(const BufferPos *, const WindowInfo *);
int bf_bp_at_screen_line_end(const BufferPos *, const WindowInfo *);
int bf_bp_move_past_buffer_extremes(const BufferPos *, Direction);
int bf_selection_started(const Buffer *);
Status bf_set_bp(Buffer *, const BufferPos *);
Status bf_change_line(Buffer *, BufferPos *, Direction, int);
Status bf_change_multi_line(Buffer *, BufferPos *, Direction, size_t, int);
Status bf_change_char(Buffer *, BufferPos *, Direction, int);
Status bf_change_multi_char(Buffer *, BufferPos *, Direction, size_t, int);
Status bf_bp_to_line_start(Buffer *, BufferPos *, int, int);
Status bf_bp_to_screen_line_start(Buffer *, BufferPos *, int, int);
Status bf_to_line_start(Buffer *, BufferPos *, int, int);
Status bf_to_line_end(Buffer *, int);
Status bf_bp_to_line_end(Buffer *, BufferPos *, int, int);
Status bf_bp_to_screen_line_end(Buffer *, BufferPos *, int, int);
Status bf_to_next_word(Buffer *, int);
Status bf_to_prev_word(Buffer *, int);
Status bf_to_buffer_start(Buffer *, int);
Status bf_to_buffer_end(Buffer *, int);
Status bf_change_page(Buffer *, Direction);
Status bf_insert_character(Buffer *, const char *, int);
Status bf_insert_string(Buffer *, const char *, size_t, int);
Status bf_replace_string(Buffer *, size_t, const char *, size_t, int);
Status bf_delete(Buffer *, size_t);
Status bf_delete_character(Buffer *);
Status bf_select_continue(Buffer *);
Status bf_select_reset(Buffer *);
Status bf_delete_range(Buffer *, const Range *);
Status bf_select_all_text(Buffer *);
Status bf_copy_selected_text(Buffer *, TextSelection *);
Status bf_cut_selected_text(Buffer *, TextSelection *);
Status bf_insert_textselection(Buffer *, TextSelection *);
void bf_free_textselection(TextSelection *);
Status bf_delete_word(Buffer *);
Status bf_delete_prev_word(Buffer *);
Status bf_set_text(Buffer *, const char *);

#endif
