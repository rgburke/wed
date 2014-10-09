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

#define FILE_BUF_SIZE 512
#define LINE_ALLOC 32

typedef enum {
    CCLASS_WHITESPACE,
    CCLASS_PUNCTUATION,
    CCLASS_WORD
} CharacterClass;

typedef struct Line Line;
typedef struct Buffer Buffer;

/* Each line of a buffer is stored in a Line structure */
struct Line {
    char *text; /* Where the actual line content is stored */
    size_t length; /* The number of bytes of text used */
    size_t screen_length; /* The number of columns on the screen text uses */
    size_t alloc_num; /* alloc_num * LINE_ALLOC = bytes allocated for text */
    int is_dirty; /* Does this line need to be redrawn */
    Line *prev; /* NULL if this is the first line in the buffer */
    Line *next; /* NULL if this is the last line in the buffer */
};

/* Used to represent a position in a buffer */
typedef struct {
    Line *line;
    size_t offset;
} BufferPos;

/* The in memory representation of a file */
struct Buffer {
    FileInfo file_info; /* stat like info */
    Line *lines; /* The first line in a doubly linked list of lines */
    BufferPos pos; /* The cursor position */
    BufferPos screen_start; /* The first screen line (can start on wrapped line) to start drawing from */
    Buffer *next; /* Next buffer in this session */
    size_t line_col_offset; /* Global cursor line offset */
};

Buffer *new_buffer(FileInfo);
void free_buffer(Buffer *);
Line *new_line(void);
Line *new_sized_line(size_t);
void free_line(Line *);
int init_bufferpos(BufferPos *);
Buffer *new_empty_buffer(void);
void resize_line_text(Line *, size_t);
void resize_line_text_if_req(Line *, size_t);
Status load_buffer(Buffer *);
size_t get_pos_line_number(Buffer *);
size_t get_pos_col_number(Buffer *);
Line *get_line_from_offset(Line *, int, size_t);
CharacterClass character_class(const char *);
const char *pos_character(Buffer *);
const char *pos_offset_character(Buffer *, int, size_t);
int pos_at_line_start(Buffer *);
int pos_at_line_end(Buffer *);
int pos_at_buffer_start(Buffer *);
int pos_at_buffer_end(Buffer *);
int pos_at_buffer_extreme(Buffer *);
Status pos_change_line(Buffer *, BufferPos *, int);
Status pos_change_muti_line(Buffer *, BufferPos *, int, size_t);
Status pos_change_char(Buffer *, BufferPos *, int, int);
Status pos_change_multi_char(Buffer *, BufferPos *, int, size_t, int);
Status pos_change_screen_line(Buffer *, BufferPos *, int, int);
Status pos_change_multi_screen_line(Buffer *, BufferPos *, int, size_t, int);
Status pos_to_screen_line_start(Buffer *);
Status pos_to_screen_line_end(Buffer *);
Status pos_to_next_word(Buffer *);
Status pos_to_prev_word(Buffer *);
Status pos_to_buffer_start(Buffer *);
Status pos_to_buffer_end(Buffer *);
Status insert_character(Buffer *, char *);
Status insert_string(Buffer *, char *, size_t, int);
Status delete_character(Buffer *);
Status delete_line(Buffer *, Line *);
Status insert_line(Buffer *);

#endif
