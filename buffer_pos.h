/*
 * Copyright (C) 2015 Richard Burke
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

#ifndef WED_BUFFER_POS_H
#define WED_BUFFER_POS_H

#include "gap_buffer.h"
#include "encoding.h"
#include "hashmap.h"

/* Represents position in buffer.
 * Each instance is specific to a buffer */
struct BufferPos {
    const GapBuffer *data; /* Underlying gap buffer that stores text */
    const FileFormat *file_format; /* Reference to file format buffer uses */
    const HashMap *config; /* Reference to buffers config */
    size_t offset; /* Offset into text */
    size_t line_no; /* Corresponding line number for this offset */
    size_t col_no; /* Corresponding column number for this offset */
};

typedef struct BufferPos BufferPos;

typedef enum {
    MP_NONE = 0,
    MP_ADJUST_OFFSET_ONLY = 1,
    MP_NO_ADJUST_ON_BUFFER_POS = 1 << 1 
} MarkProperties;

typedef struct {
    BufferPos *pos;
    MarkProperties prop;
} Mark;

/* Represent selected text in a buffer,
 * start is inclusive, end is exclusive */
typedef struct {
    BufferPos start;
    BufferPos end;
} Range;

int bp_init(BufferPos *, const GapBuffer *, const FileFormat *,
            const HashMap *config);
Mark *bp_new_mark(BufferPos *, MarkProperties);
void bp_free_mark(Mark *);
char bp_get_char(const BufferPos *);
unsigned char bp_get_uchar(const BufferPos *);
int bp_compare(const BufferPos *, const BufferPos *);
BufferPos bp_min(const BufferPos *, const BufferPos *);
BufferPos bp_max(const BufferPos *, const BufferPos *);
int bp_at_line_start(const BufferPos *);
int bp_at_line_end(const BufferPos *);
int bp_on_empty_line(const BufferPos *);
int bp_on_whitespace_line(const BufferPos *);
int bp_at_first_line(const BufferPos *);
int bp_at_last_line(const BufferPos *);
int bp_at_buffer_start(const BufferPos *);
int bp_at_buffer_end(const BufferPos *);
int bp_at_buffer_extreme(const BufferPos *);
void bp_next_char(BufferPos *);
void bp_prev_char(BufferPos *);
void bp_to_line_start(BufferPos *);
void bp_to_line_end(BufferPos *);
void bp_recalc_col(BufferPos *);
int bp_next_line(BufferPos *);
int bp_prev_line(BufferPos *);
void bp_to_buffer_start(BufferPos *);
void bp_to_buffer_end(BufferPos *);
void bp_advance_to_offset(BufferPos *, size_t offset);
void bp_reverse_to_offset(BufferPos *, size_t offset);
void bp_advance_to_line(BufferPos *, size_t line_no);
void bp_reverse_to_line(BufferPos *, size_t line_no, int end_of_line);
void bp_advance_to_col(BufferPos *, size_t col_no);
void bp_reverse_to_col(BufferPos *, size_t col_no);
void bp_advance_to_line_col(BufferPos *, size_t line_no, size_t col_no);
void bp_reverse_to_line_col(BufferPos *, size_t line_no, size_t col_no);
BufferPos bp_init_from_offset(size_t offset, const BufferPos *known_pos);
BufferPos bp_init_from_line_col(size_t line_no, size_t col_no,
                                const BufferPos *known_pos);

#endif
