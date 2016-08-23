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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "buffer_pos.h"
#include "util.h"
#include "status.h"

#define CORRECT_LINE_NO(lineno, maxlineno) \
        ((lineno) == 0 ? 1 : MIN((lineno), (maxlineno)))
#define CORRECT_COL_NO(colno) ((colno) == 0 ? 1 : (colno))

typedef enum {
    NP_BUFFER_START,
    NP_KNOWN_POS,
    NP_BUFFER_END,
    NP_ENTRY_NUM
} NearestPos;

static int bp_is_char_before(const BufferPos *, size_t offset, char ch);
static void calc_new_col(BufferPos *, size_t new_offset);
static NearestPos bp_determine_nearest_pos(size_t pos, size_t start, 
                                           size_t known, size_t end);

int bp_init(BufferPos *pos, const GapBuffer *data, 
            const FileFormat *file_format,
            const HashMap *config)
{
    assert(pos != NULL);
    assert(data != NULL);
    assert(file_format != NULL);
    assert(config != NULL);

    pos->offset = 0;
    pos->line_no = 1;
    pos->col_no = 1;
    pos->data = data;
    pos->file_format = file_format;
    pos->config = config;

    return 1;
}

Mark *bp_new_mark(BufferPos *pos, MarkProperties prop)
{
    Mark *mark = malloc(sizeof(Mark));
    RETURN_IF_NULL(mark);

    memset(mark, 0, sizeof(Mark));
    mark->pos = pos;
    mark->prop = prop;

    return mark;
}

void bp_free_mark(Mark *mark)
{
    if (mark != NULL) {
        free(mark);
    }
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

int bp_on_empty_line(const BufferPos *pos)
{
    return bp_at_line_start(pos) && bp_at_line_end(pos);
}

int bp_on_whitespace_line(const BufferPos *pos)
{
    BufferPos tmp = *pos;
    bp_to_line_start(&tmp);

    while (!bp_at_line_end(&tmp)) {
        if (!isspace(bp_get_char(&tmp))) {
            return 0;
        }

        bp_next_char(&tmp);
    }

    return 1;
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
        en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, 
                          pos, pos->config);
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
        size_t prev_offset = en_utf8_previous_char_offset(pos);
        pos->offset -= prev_offset;

        if (bp_get_char(pos) == '\t') {
            bp_recalc_col(pos);
        } else {
            CharInfo char_info;
            en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, 
                              pos, pos->config);

            if (char_info.byte_length == prev_offset) {
                pos->col_no -= char_info.screen_length;
            } else {
                /* Invalid UTF-8 byte sequence encountered.
                 * Ensure we don't skip back too far */
                size_t remaining_bytes = prev_offset - char_info.byte_length;    

                while (remaining_bytes > 0) {
                    pos->offset += char_info.byte_length; 
                    en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, 
                                      pos, pos->config);
                    remaining_bytes -= char_info.byte_length;
                }

                pos->col_no -= char_info.screen_length;
            }
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
        en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, 
                          pos, pos->config);
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

    if (gb_find_prev(pos->data, pos->offset, &offset, '\n')) {
        if (pos->line_no == 2) {
            bp_to_buffer_start(pos);
            return 1;
        } else if (offset > 0 &&
                   gb_find_prev(pos->data, offset, &offset, '\n')) {
            pos->offset = offset + 1;
            pos->line_no--;
            pos->col_no = 1;
            return 1;
        }
    }

    return 0;
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
    /* gb_lines returns the number of line endings
     * i.e. 0 line endings means we're on line 1 */
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

void bp_advance_to_line(BufferPos *pos, size_t line_no)
{
    size_t lines = gb_lines(pos->data) + 1;
    line_no = CORRECT_LINE_NO(line_no, lines);

    while (pos->line_no < line_no) {
        if (!bp_next_line(pos)) {
            break;
        }
    }
}

void bp_reverse_to_line(BufferPos *pos, size_t line_no, int end_of_line)
{
    size_t lines = gb_lines(pos->data) + 1;
    line_no = CORRECT_LINE_NO(line_no, lines);
    /* end_of_line reverses to the end of the line we want
     * instead of the start */

    if (end_of_line) {
        line_no++;
    }

    while (pos->line_no > line_no) {
        if (!bp_prev_line(pos)) {
            break;
        }
    }

    if (end_of_line) {
        bp_to_line_start(pos);
        bp_prev_char(pos);
    }
}

void bp_advance_to_col(BufferPos *pos, size_t col_no)
{
    col_no = CORRECT_COL_NO(col_no);

    while (pos->col_no < col_no && !bp_at_line_end(pos)) {
        bp_next_char(pos);
    }
}

void bp_reverse_to_col(BufferPos *pos, size_t col_no)
{
    col_no = CORRECT_COL_NO(col_no);

    while (pos->col_no > col_no && !bp_at_line_start(pos)) {
        bp_prev_char(pos);
    }
}

void bp_advance_to_line_col(BufferPos *pos, size_t line_no, size_t col_no)
{
    size_t lines = gb_lines(pos->data) + 1;
    line_no = CORRECT_LINE_NO(line_no, lines);
    col_no = CORRECT_COL_NO(col_no);
    
    bp_advance_to_line(pos, line_no);
    bp_advance_to_col(pos, col_no);
}

void bp_reverse_to_line_col(BufferPos *pos, size_t line_no, size_t col_no)
{
    size_t lines = gb_lines(pos->data) + 1;
    line_no = CORRECT_LINE_NO(line_no, lines);
    col_no = CORRECT_COL_NO(col_no);
    
    bp_reverse_to_line(pos, line_no, 1);
    bp_reverse_to_col(pos, col_no);
}

BufferPos bp_init_from_offset(size_t offset, const BufferPos *known_pos)
{
    size_t buffer_len = gb_length(known_pos->data);
    offset = MIN(offset, buffer_len);
    BufferPos pos = *known_pos;

    NearestPos nearest_pos = bp_determine_nearest_pos(offset, 0,
                                                      known_pos->offset,
                                                      buffer_len);

    if (nearest_pos == NP_BUFFER_END) {
        bp_to_buffer_end(&pos);
        bp_reverse_to_offset(&pos, offset);    
    } else if (nearest_pos == NP_KNOWN_POS) {
        if (known_pos->offset > offset) {
            bp_reverse_to_offset(&pos, offset);    
        } else if (known_pos->offset < offset) {
            bp_advance_to_offset(&pos, offset);
        }
    } else {
        bp_to_buffer_start(&pos);
        bp_advance_to_offset(&pos, offset);
    }

    return pos;
}

BufferPos bp_init_from_line_col(size_t line_no, size_t col_no, 
                                const BufferPos *known_pos)
{
    size_t lines = gb_lines(known_pos->data) + 1;
    line_no = CORRECT_LINE_NO(line_no, lines);
    col_no = CORRECT_COL_NO(col_no);

    NearestPos nearest_pos = bp_determine_nearest_pos(line_no, 1,
                                                      known_pos->line_no,
                                                      lines);
    BufferPos pos = *known_pos;

    if (nearest_pos == NP_BUFFER_END) {
        bp_to_buffer_end(&pos);
        bp_reverse_to_line_col(&pos, line_no, col_no);
    } else if (nearest_pos == NP_KNOWN_POS) {
        if (line_no < known_pos->line_no) {
            bp_reverse_to_line_col(&pos, line_no, col_no);
        } else if (line_no > known_pos->line_no) {
            bp_advance_to_line_col(&pos, line_no, col_no);
        } else {
            if (col_no > known_pos->col_no) {
                bp_advance_to_col(&pos, col_no);
            } else if (col_no < known_pos->col_no) {
                bp_reverse_to_col(&pos, col_no);
            }
        }
    } else {
        bp_to_buffer_start(&pos);
        bp_advance_to_line_col(&pos, line_no, col_no);
    }

    return pos;
}

static NearestPos bp_determine_nearest_pos(size_t pos, size_t start, 
                                           size_t known, size_t end)
{
    size_t pos_diffs[NP_ENTRY_NUM] = {
        [NP_BUFFER_START] = ABS_DIFF(pos, start),
        [NP_KNOWN_POS]    = ABS_DIFF(pos, known),
        [NP_BUFFER_END]   = ABS_DIFF(pos, end)
    };

    NearestPos nearest_pos = NP_BUFFER_START;

    for (size_t k = 1; k < NP_ENTRY_NUM; k++) {
        if (pos_diffs[k] < pos_diffs[nearest_pos]) {
            nearest_pos = k;
        }
    }

    return nearest_pos;
}
