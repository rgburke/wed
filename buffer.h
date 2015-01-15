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
#include "status.h"
#include "file.h"
#include "hashmap.h"
#include "encoding.h"

#define FILE_BUF_SIZE 512
#define LINE_ALLOC 32

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

typedef enum {
    TST_STRING,
    TST_LINE
} TextSelectionType;

typedef struct Line Line;
typedef struct Buffer Buffer;

/* Each line of a buffer is stored in a Line structure */
struct Line {
    char *text; /* Where the actual line content is stored */
    Line *prev; /* NULL if this is the first line in the buffer */
    Line *next; /* NULL if this is the last line in the buffer */
    size_t length; /* The number of bytes of text used */
    size_t screen_length; /* The number of columns on the screen text uses */
    size_t alloc_num; /* alloc_num * LINE_ALLOC = bytes allocated for text */
};

/* Represent a position in a buffer */
struct BufferPos {
    Line *line;
    size_t offset;
    size_t line_no;
    size_t col_no;
};

typedef struct BufferPos BufferPos;

/* Represent selected text in a buffer,
 * start is inclusive, end is exclusive */
typedef struct {
    BufferPos start;
    BufferPos end;
} Range;

typedef struct {
    size_t height;
    size_t width;
    size_t start_y;
    size_t start_x;
    size_t win_index;
} WindowInfo;

/* The in memory representation of a file */
struct Buffer {
    FileInfo file_info; /* stat like info */
    Line *lines; /* The first line in a doubly linked list of lines */
    BufferPos pos; /* The cursor position */
    BufferPos screen_start; /* The first screen line (can start on wrapped line) to start drawing from */
    BufferPos select_start; /* Starting position of selected text */
    Buffer *next; /* Next buffer in this session */
    size_t line_col_offset; /* Global cursor line offset */
    WindowInfo win_info; /* Window dimension info */
    HashMap *config; /* Stores config variables */
    CharacterEncodingType encoding_type;
    CharacterEncodingFunctions cef;
};

typedef struct {
    TextSelectionType type;
    union {
        char *string;
        Line *lines;
    } text;
} TextSelection;

Buffer *new_buffer(FileInfo);
Buffer *new_empty_buffer(void);
void free_buffer(Buffer *);
Line *new_line(void);
Line *new_sized_line(size_t);
void free_line(Line *);
int init_bufferpos(BufferPos *);
TextSelection *new_textselection(Buffer *, Range);
void free_textselection(TextSelection *);
Line *clone_line(Line *line);
void resize_line_text(Line *, size_t);
void resize_line_text_if_req(Line *, size_t);
Status clear_buffer(Buffer *);
Status load_buffer(Buffer *);
Status write_buffer(Buffer *);
size_t buffer_byte_num(Buffer *);
size_t buffer_line_num(Buffer *);
char *get_buffer_as_string(Buffer *);
int buffer_file_exists(Buffer *);
int has_file_path(Buffer *);
int set_buffer_file_path(Buffer *, const char *);
Line *get_line_from_offset(Line *, Direction, size_t);
int bufferpos_compare(BufferPos, BufferPos);
BufferPos bufferpos_min(BufferPos, BufferPos);
BufferPos bufferpos_max(BufferPos, BufferPos);
int get_selection_range(Buffer *, Range *);
int bufferpos_in_range(Range, BufferPos);
size_t range_length(Buffer *, Range);
CharacterClass character_class(Buffer *, BufferPos);
const char *pos_character(Buffer *);
const char *pos_offset_character(Buffer *, Direction, size_t);
char *get_line_segment(Line *, size_t, size_t);
Line *clone_line_segment(Buffer *, BufferPos, BufferPos);
int bufferpos_at_line_start(BufferPos);
int bufferpos_at_screen_line_start(BufferPos, WindowInfo);
int bufferpos_at_line_end(BufferPos);
int bufferpos_at_screen_line_end(BufferPos, WindowInfo);
int bufferpos_at_first_line(BufferPos);
int bufferpos_at_last_line(BufferPos);
int bufferpos_at_buffer_start(BufferPos);
int bufferpos_at_buffer_end(BufferPos);
int bufferpos_at_buffer_extreme(BufferPos);
int move_past_buffer_extremes(BufferPos, Direction);
int selection_started(Buffer *);
Status pos_change_line(Buffer *, BufferPos *, Direction, int);
Status pos_change_multi_line(Buffer *, BufferPos *, Direction, size_t, int);
Status pos_change_char(Buffer *, BufferPos *, Direction, int);
Status pos_change_multi_char(Buffer *, BufferPos *, Direction, size_t, int);
Status bpos_to_line_start(Buffer *, BufferPos *, int, int);
Status bpos_to_screen_line_start(Buffer *, BufferPos *, int, int);
Status pos_to_line_start(Buffer *, int);
Status pos_to_line_end(Buffer *, int);
Status pos_to_next_word(Buffer *, int);
Status pos_to_prev_word(Buffer *, int);
Status pos_to_buffer_start(Buffer *, int);
Status pos_to_buffer_end(Buffer *, int);
Status pos_to_bufferpos(Buffer *, BufferPos);
Status pos_change_page(Buffer *, Direction);
Status insert_character(Buffer *, const char *);
Status insert_string(Buffer *, char *, size_t, int);
Status delete_character(Buffer *);
Status delete_line(Buffer *, Line *);
Status insert_line(Buffer *);
Status select_continue(Buffer *);
Status select_reset(Buffer *);
Status delete_range(Buffer *, Range);
Status select_all_text(Buffer *);
Status copy_selected_text(Buffer *, TextSelection **);
Status cut_selected_text(Buffer *, TextSelection **);
Status insert_textselection(Buffer *, TextSelection *);

#endif
