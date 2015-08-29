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

struct BufferPos {
    const GapBuffer *data;
    const CEF *cef;
    const FileFormat *file_format;
    size_t offset;
    size_t line_no;
    size_t col_no;
};

typedef struct BufferPos BufferPos;

int bp_init(BufferPos *, const GapBuffer *, const CEF *, const FileFormat *);
char bp_get_char(const BufferPos *);
unsigned char bp_get_uchar(const BufferPos *);
int bp_compare(const BufferPos *, const BufferPos *);
BufferPos bp_min(const BufferPos *, const BufferPos *);
BufferPos bp_max(const BufferPos *, const BufferPos *);
int bp_at_line_start(const BufferPos *);
int bp_at_line_end(const BufferPos *);
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
void bp_advance_to_col(BufferPos *, size_t);
void bp_reverse_to_col(BufferPos *, size_t);
void bp_to_buffer_start(BufferPos *);
void bp_to_buffer_end(BufferPos *);
void bp_advance_to_offset(BufferPos *, size_t);
void bp_reverse_to_offset(BufferPos *, size_t);
BufferPos bp_init_from_offset(size_t, const BufferPos *);

#endif
