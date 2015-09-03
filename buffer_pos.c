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

#include "buffer_pos.h"
#include "util.h"
#include <assert.h>

static int bp_is_char_before(const BufferPos *, size_t, char);
static void calc_new_col(BufferPos *, size_t);

int bp_init(BufferPos *pos, const GapBuffer *data, 
            const CEF *cef, const FileFormat *file_format,
            const HashMap *config)
{
    assert(pos != NULL);
    assert(data != NULL);
    assert(cef != NULL);
    assert(file_format != NULL);
    assert(config != NULL);

    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;
    pos->data = data;
    pos->cef = cef;
    pos->file_format = file_format;
    pos->config = config;

    return 1;
}

char bp_get_char(const BufferPos *pos)
{
    return gb_get_at(pos->data, pos->offset);
}

unsigned char bp_get_uchar(const BufferPos *pos)
{
    return gb_getu_at(pos->data, pos->offset);
}

static int bp_is_char_before(const BufferPos *pos, size_t offset, char ch)
{
    return pos->offset >= offset &&
           gb_get_at(pos->data, pos->offset - offset) == ch;
}

int bp_compare(const BufferPos *pos1, const BufferPos *pos2)
{
    if (pos1->line_no == pos2->line_no) {
        return (pos1->col_no < pos2->col_no ? -1 : pos1->col_no > pos2->col_no);
    }

    return (pos1->line_no < pos2->line_no ? -1 : pos1->line_no > pos2->line_no);
}

BufferPos bp_min(const BufferPos *pos1, const BufferPos *pos2)
{
    if (bp_compare(pos1, pos2) == -1) {
        return *pos1;
    }

    return *pos2;
}

BufferPos bp_max(const BufferPos *pos1, const BufferPos *pos2)
{
    if (bp_compare(pos1, pos2) == 1) {
        return *pos1;
    }

    return *pos2;
}

int bp_at_line_start(const BufferPos *pos)
{
    if (pos->offset == 0) {
        return 1;
    }

    return gb_get_at(pos->data, pos->offset - 1) == '\n';
}

int bp_at_line_end(const BufferPos *pos)
{
    size_t buffer_len = gb_length(pos->data);

    if (pos->offset == buffer_len) {
        return 1;
    }

    if (*pos->file_format == FF_WINDOWS &&
        bp_get_char(pos) == '\r' &&
        pos->offset + 1 < buffer_len &&
        gb_get_at(pos->data, pos->offset + 1) == '\n') {
        return 1;
    }

    return bp_get_char(pos) == '\n';
}

int bp_at_first_line(const BufferPos *pos)
{
    return pos->line_no == 1;
}

int bp_at_last_line(const BufferPos *pos)
{
    return pos->line_no == gb_lines(pos->data) + 1;
}

int bp_at_buffer_start(const BufferPos *pos)
{
    return bp_at_first_line(pos) && bp_at_line_start(pos);
}

int bp_at_buffer_end(const BufferPos *pos)
{
    return bp_at_last_line(pos) && bp_at_line_end(pos);
}

int bp_at_buffer_extreme(const BufferPos *pos)
{
    return bp_at_buffer_start(pos) || bp_at_buffer_end(pos);
}

void bp_next_char(BufferPos *pos)
{
    if (bp_at_buffer_end(pos)) {
        return;
    }

    if (bp_at_line_end(pos)) {
        if (*pos->file_format == FF_WINDOWS &&
            bp_get_char(pos) == '\r') {
            pos->offset++;
        }

        pos->offset++;
        pos->line_no++;
        pos->col_no = 1;
    } else {
        CharInfo char_info;
        pos->cef->char_info(&char_info, CIP_SCREEN_LENGTH, 
                            *pos, pos->config);
        pos->offset += char_info.byte_length;
        pos->col_no += char_info.screen_length;
    }
}

void bp_prev_char(BufferPos *pos)
{
    if (bp_at_buffer_start(pos)) {
        return;
    } 

    if (bp_at_line_start(pos)) {
        pos->offset--;
        pos->line_no--;

        if (*pos->file_format == FF_WINDOWS &&
            bp_is_char_before(pos, 1, '\r')) {
            pos->offset--;
        }

        bp_recalc_col(pos);
    } else {
        size_t prev_offset = pos->cef->previous_char_offset(*pos);
        pos->offset -= prev_offset;

        if (bp_get_char(pos) == '\t') {
            bp_recalc_col(pos);
        } else {
            CharInfo char_info;
            pos->cef->char_info(&char_info, CIP_SCREEN_LENGTH, 
                                *pos, pos->config);
            pos->col_no -= char_info.screen_length;
        }
    }
}

void bp_to_line_start(BufferPos *pos)
{
    if (bp_at_line_start(pos)) {
        pos->col_no = 1;
        return;
    }

    if (gb_find_prev(pos->data, pos->offset, &pos->offset, '\n')) {
        pos->offset++;
    } else {
        pos->offset = 0;
    }

    pos->col_no = 1;
}

void bp_to_line_end(BufferPos *pos)
{
    if (bp_at_line_end(pos)) {
        return;
    }

    size_t line_end_offset;

    if (gb_find_next(pos->data, pos->offset, &line_end_offset, '\n')) {
        if (*pos->file_format == FF_WINDOWS &&
            line_end_offset > 0 &&
            gb_get_at(pos->data, line_end_offset - 1) == '\r') {
            line_end_offset--;
        }
    } else {
        line_end_offset = gb_length(pos->data);
    }

    calc_new_col(pos, line_end_offset);
}

void bp_recalc_col(BufferPos *pos)
{
    BufferPos tmp = *pos; 
    bp_to_line_start(&tmp);
    calc_new_col(&tmp, pos->offset);
    *pos = tmp;
}

static void calc_new_col(BufferPos *pos, size_t new_offset)
{
    CharInfo char_info;

    while (pos->offset < new_offset) {
        pos->cef->char_info(&char_info, CIP_SCREEN_LENGTH, 
                            *pos, pos->config);
        pos->col_no += char_info.screen_length;
        pos->offset += char_info.byte_length;
    }
}

int bp_next_line(BufferPos *pos)
{
    if (gb_find_next(pos->data, pos->offset, &pos->offset, '\n')) {
        pos->offset++;
        pos->line_no++;
        pos->col_no = 1;
        return 1;
    }

    return 0;
}

int bp_prev_line(BufferPos *pos)
{
    size_t offset;

    if (gb_find_prev(pos->data, pos->offset, &offset, '\n') &&
        offset > 0 && gb_find_prev(pos->data, offset, &offset, '\n')) {
        pos->offset = offset + 1;
        pos->line_no--;
        pos->col_no = 1;
        return 1;
    }

    return 0;
}

void bp_advance_to_col(BufferPos *pos, size_t col_no)
{
    while (pos->col_no < col_no && !bp_at_line_end(pos)) {
        bp_next_char(pos);
    }
}

void bp_reverse_to_col(BufferPos *pos, size_t col_no)
{
    while (pos->col_no > col_no && !bp_at_line_start(pos)) {
        bp_prev_char(pos);
    }
}

void bp_to_buffer_start(BufferPos *pos)
{
    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;
}

void bp_to_buffer_end(BufferPos *pos)
{
    pos->offset = gb_length(pos->data);
    pos->line_no = gb_lines(pos->data) + 1;
    bp_recalc_col(pos);
}

void bp_advance_to_offset(BufferPos *pos, size_t offset)
{
    BufferPos tmp = *pos;
    offset = MIN(offset, gb_length(pos->data));

    while (tmp.offset < offset) {
        *pos = tmp;

        if (!bp_next_line(&tmp)) {
            break;
        }
    }

    if (tmp.offset == offset) {
        pos->line_no = tmp.line_no;
    }

    pos->offset = offset;
    bp_recalc_col(pos);
}

void bp_reverse_to_offset(BufferPos *pos, size_t offset)
{
    BufferPos tmp = *pos;

    bp_to_line_start(&tmp);

    while (tmp.offset > offset) {
        *pos = tmp;

        if (!bp_prev_line(&tmp)) {
            break;
        }
    }

    if (tmp.offset <= offset) {
        pos->line_no = tmp.line_no;
    }

    pos->offset = offset;
    bp_recalc_col(pos);
}

BufferPos bp_init_from_offset(size_t offset, const BufferPos *known_pos)
{
    size_t buffer_len = gb_length(known_pos->data);
    offset = MIN(offset, buffer_len);
    BufferPos pos = *known_pos;

    if (known_pos->offset > offset &&
        known_pos->offset - offset < offset) {
        bp_reverse_to_offset(&pos, offset);    
    } else if (known_pos->offset < offset) {
        bp_advance_to_offset(&pos, offset);
    } else if (known_pos->offset != offset) {
        bp_to_buffer_start(&pos);
        bp_advance_to_offset(&pos, offset);
    }

    return pos;
}
