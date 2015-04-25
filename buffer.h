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

#define DIRECTION_OFFSET(d) (((d) & 1) ? -1 : 1)

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
};

Buffer *new_buffer(FileInfo);
Buffer *new_empty_buffer(const char *);
void free_buffer(Buffer *);
Status clear_buffer(Buffer *);
Status update_screen_length(Buffer *);
Status load_buffer(Buffer *);
Status write_buffer(Buffer *, const char *);
char *get_buffer_as_string(Buffer *);
char *join_lines(Buffer *, const char *);
int buffer_is_empty(Buffer *);
int buffer_file_exists(Buffer *);
int bufferpos_compare(BufferPos, BufferPos);
BufferPos bufferpos_min(BufferPos, BufferPos);
BufferPos bufferpos_max(BufferPos, BufferPos);
int get_selection_range(Buffer *, Range *);
int bufferpos_in_range(Range, BufferPos);
CharacterClass character_class(const BufferPos *);
const char *pos_offset_character(Buffer *, BufferPos, Direction, size_t);
int bufferpos_at_line_start(BufferPos);
int bufferpos_at_screen_line_start(const BufferPos *, WindowInfo);
int bufferpos_at_line_end(BufferPos);
int bufferpos_at_screen_line_end(const BufferPos *, WindowInfo);
int bufferpos_at_first_line(BufferPos);
int bufferpos_at_last_line(BufferPos);
int bufferpos_at_buffer_start(BufferPos);
int bufferpos_at_buffer_end(BufferPos);
int bufferpos_at_buffer_extreme(BufferPos);
int move_past_buffer_extremes(const BufferPos *, Direction);
int selection_started(Buffer *);
Status pos_change_line(Buffer *, BufferPos *, Direction, int);
Status pos_change_multi_line(Buffer *, BufferPos *, Direction, size_t, int);
Status pos_change_char(Buffer *, BufferPos *, Direction, int);
Status pos_change_multi_char(Buffer *, BufferPos *, Direction, size_t, int);
Status bpos_to_line_start(Buffer *, BufferPos *, int, int);
Status bpos_to_screen_line_start(Buffer *, BufferPos *, int, int);
Status pos_to_line_start(Buffer *, BufferPos *, int, int);
Status pos_to_line_end(Buffer *, int);
Status bpos_to_screen_line_end(Buffer *, BufferPos *, int, int);
Status pos_to_next_word(Buffer *, int);
Status pos_to_prev_word(Buffer *, int);
Status pos_to_buffer_start(Buffer *, int);
Status pos_to_buffer_end(Buffer *, int);
Status pos_to_bufferpos(Buffer *, BufferPos);
Status pos_change_page(Buffer *, Direction);
Status insert_character(Buffer *, const char *, int);
Status insert_string(Buffer *, const char *, size_t, int);
Status delete_character(Buffer *);
Status select_continue(Buffer *);
Status select_reset(Buffer *);
Status delete_range(Buffer *, Range);
Status select_all_text(Buffer *);
Status copy_selected_text(Buffer *, TextSelection *);
Status cut_selected_text(Buffer *, TextSelection *);
Status insert_textselection(Buffer *, TextSelection *);
void free_textselection(TextSelection *);
Status delete_word(Buffer *);
Status delete_prev_word(Buffer *);

#endif
