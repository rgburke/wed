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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "buffer_view.h"
#include "buffer.h"
#include "config.h"
#include "util.h"

#define SYNTAX_CACHE_LINES 10

static int bv_vertical_scroll_linewrap(Buffer *);
static int bv_vertical_scroll(Buffer *);
static int bv_horizontal_scroll(Buffer *);

static SyntaxMatches *bv_get_syntax_matches(const Session *, Buffer *,
                                            const BufferPos *draw_pos);
static int bv_can_use_syntax_match_cache(Buffer *, const BufferPos *draw_pos);

static void bv_set_cell(Cell *, size_t offset, size_t col_no, size_t col_width,
                        CellAttribute, const char *fmt, ...);
static void bv_populate_buffer_data(const Buffer *);
static void bv_clear_view(BufferView *);
static void bv_populate_syntax_data(const Session *, Buffer *);
static void bv_populate_selection_data(Buffer *);
static void bv_populate_colorcolumn_data(Buffer *);
static void bv_populate_cursor_data(Buffer *);

BufferView *bv_new(size_t rows, size_t cols, const BufferPos *screen_start)
{
    assert(rows > 0);
    assert(cols > 0);
    assert(screen_start != NULL);

    BufferView *bv = calloc(1, sizeof(BufferView));
    RETURN_IF_NULL(bv);

    bv->lines = calloc(rows, sizeof(Line));

    if (bv->lines == NULL) {
        bv_free(bv);
        return NULL;
    }

    bv->rows = rows;
    bv->cols = cols;
    bv->screen_start = *screen_start;

    Line *line;

    for (size_t row = 0; row < rows; row++) {
        line = &bv->lines[row];
        line->cells = calloc(cols, sizeof(Cell));

        if (line->cells == NULL) {
            bv_free(bv);
            return NULL;
        }
    }

    bv->rows_allocated = rows;
    bv->cols_allocated = cols;

    return bv;
}

void bv_free(BufferView *bv)
{
    if (bv == NULL) {
        return;
    }

    bv_free_syntax_match_cache(bv);

    if (bv->lines != NULL) {
        for (size_t row = 0; row < bv->rows_allocated; row++) {
            free(bv->lines[row].cells);
        }
    }

    free(bv->lines);
    free(bv);
}

void bv_update_view(const Session *sess, Buffer *buffer)
{
    int line_wrap = cf_bool(buffer->config, CV_LINEWRAP);
    int scrolled;

    if (line_wrap) {
        scrolled = bv_vertical_scroll_linewrap(buffer);
    } else {
        scrolled = bv_vertical_scroll(buffer);
        scrolled |= bv_horizontal_scroll(buffer);
    }

    if (bf_is_draw_dirty(buffer) || scrolled || buffer->bv->resized) {
        bv_populate_buffer_data(buffer);
        bv_populate_syntax_data(sess, buffer);
    }

    bv_populate_selection_data(buffer);
    bv_populate_colorcolumn_data(buffer);
    bv_populate_cursor_data(buffer);
}

static int bv_vertical_scroll_linewrap(Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &bv->screen_start;
    int scrolled = 0;
    bv->horizontal_scroll = 0;

    if ((pos.line_no < screen_start->line_no) ||
        (pos.line_no == screen_start->line_no &&
         pos.col_no < screen_start->col_no)) {
        /* Start displaying from the same line pos is on, as it's
         * before our current screen_start */
        *screen_start = pos;

        if (!bf_bp_at_screen_line_start(buffer, screen_start)) {
            bf_bp_to_screen_line_start(buffer, screen_start, 0, 0);
        }

        scrolled = 1;
    } else {
        BufferPos start = pos;

        if (!bf_bp_at_screen_line_start(buffer, &start)) {
            bf_bp_to_screen_line_start(buffer, &start, 0, 0);
        }

        size_t line_num = bv->rows;

        /* Scan down as much as two screens from old screen_start to see if we
         * can find pos. Moving down a screen line is always a fast operation
         * unlike moving up a screen line. If we're moving from a line to the
         * previous line and the previous line is extremely long, the column
         * number we land on will have to be calculated and this is very costly
         * for long lines. So while the code in the if block below generally
         * isn't necessary as scanning up from pos alone is sufficient, it
         * greatly improves the performance and responsiveness of wed when
         * editing a file with very long lines. */
        if (pos.line_no <= screen_start->line_no + (line_num * 2)) {
            if (!bf_bp_at_screen_line_start(buffer, screen_start)) {
                bf_bp_to_screen_line_start(buffer, screen_start, 0, 0);
            }

            BufferPos screen_start_tmp = *screen_start;
            size_t scan_lines = line_num * 2;

            while (bp_compare(&screen_start_tmp, &start) != 0 &&
                   --scan_lines > 0) {
                bf_change_line(buffer, &screen_start_tmp, DIRECTION_DOWN, 0);
            }

            if (scan_lines > 0) {
                scrolled = scan_lines <= line_num;

                if (scrolled) {
                    for (size_t k = 0; k <= line_num - scan_lines; k++) {
                        bf_change_line(buffer, screen_start, DIRECTION_DOWN, 0);
                    }
                }

                return scrolled;
            }
        }

        /* Reverse back from pos until we encounter screen_start or traverse
         * the height of the screen. If we don't encounter screen_start
         * then start from where we traversed back to */
        BufferPos screen_start_prev = *screen_start;

        while (bp_compare(&start, screen_start) != 0 && --line_num > 0) {
            bf_change_line(buffer, &start, DIRECTION_UP, 0);
        }

        if (line_num == 0) {
            *screen_start = start;
        }

        if (bp_compare(&screen_start_prev, screen_start) != 0) {
            scrolled = 1;
        }
    }

    return scrolled;
}

/* TODO consider using ncurses scroll function as well */
/* Determine if the screen needs to be scrolled and 
 * how much if so. There is a separate scroll function
 * when linewrap is enabled */
static int bv_vertical_scroll(Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &bv->screen_start;
    bp_to_line_start(screen_start);
    int scrolled = 0;

    if (pos.line_no < screen_start->line_no) {
        /* If pos is now before the start of the buffer content we're
         * currently displaying, then start displaying the buffer from
         * the same line pos is on */
        BufferPos tmp = pos;
        bp_to_line_start(&tmp);
        screen_start->offset = tmp.offset;
        screen_start->line_no = tmp.line_no;
        scrolled = 1;
    } else {
        size_t diff = pos.line_no - screen_start->line_no;

        /* pos still appears on screen with current screen_start */
        if (diff >= bv->rows) {
            scrolled = 1;
            diff -= (bv->rows - 1);

            if (diff > bv->rows) {
                /* pos is beyond the end of the current buffer content
                 * displayed, so start displaying from same line as pos */
                BufferPos tmp = pos;
                bp_to_line_start(&tmp);
                screen_start->offset = tmp.offset;
                screen_start->line_no = tmp.line_no;
            } else {
                /* pos is only a couple of lines below the end of the buffer
                 * content displayed, so scroll screen_start down until pos
                 * comes into view. This allows us to scroll down through
                 * the buffer smoothly from the users perspective */
                bf_change_multi_line(buffer, screen_start, DIRECTION_DOWN,
                                     diff, 0);
            }
        }
    }

    return scrolled;
}

/* Only called when linewrap=false */
static int bv_horizontal_scroll(Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    BufferPos pos = buffer->pos;
    int scrolled = 0;
    Direction direction;
    size_t diff;

    /* bv->horizontal_scroll is the column we're currently
     * starting to display each line from */

    if (pos.col_no > bv->horizontal_scroll) {
        diff = pos.col_no - bv->horizontal_scroll;
        direction = DIRECTION_RIGHT;
    } else {
        diff = bv->horizontal_scroll - pos.col_no;
        direction = DIRECTION_LEFT;
    }

    if (diff == 0) {
        return scrolled;
    }

    if (direction == DIRECTION_RIGHT) {
        if (diff >= bv->cols) {
            diff -= (bv->cols - 1);
            bv->horizontal_scroll += diff;
            scrolled = 1;
        }
    } else {
        bv->horizontal_scroll -= diff;
        scrolled = 1;
    }

    return scrolled;
}

static SyntaxMatches *bv_get_syntax_matches(const Session *sess,
                                            Buffer *buffer,
                                            const BufferPos *draw_pos)
{
    const SyntaxDefinition *syn_def = se_get_syntax_def(sess, buffer);

    if (syn_def == NULL) {
        return NULL;
    }

    /* Look ahead and behind from the current visible part of the buffer
     * by up to 30 lines when determining syntax matches. This aims to ensure
     * constructs that span many lines, such as comments, which can start or
     * end outside of the visible buffer area are matched and highlighted.
     * Of course this isn't always enough for large comments and adds
     * a load overhead */
    BufferView *bv = buffer->bv;
    BufferPos syn_start = *draw_pos;
    bf_change_multi_line(buffer, &syn_start, DIRECTION_UP,
                         SYNTAX_CACHE_LINES, 0);

    for (size_t k = 0; !bp_on_empty_line(&syn_start) && k < 20; k++) {
        bf_change_line(buffer, &syn_start, DIRECTION_UP, 0); 
    }

    if (bv_can_use_syntax_match_cache(buffer, draw_pos)) {
        bv->syn_match_cache.syn_matches->current_match = 0;
        return bv->syn_match_cache.syn_matches;
    } else if (bv->syn_match_cache.syn_matches != NULL) {
        bv_free_syntax_match_cache(bv); 
    }

    BufferPos syn_end = *draw_pos;
    bf_change_multi_line(buffer, &syn_end, DIRECTION_DOWN,
                         bv->rows + SYNTAX_CACHE_LINES, 0);

    size_t syn_examine_length = syn_end.offset - syn_start.offset;
    char *syn_examine_text = malloc(syn_examine_length + 1);

    if (syn_examine_text == NULL) {
        return NULL;
    }

    syn_examine_length = gb_get_range(buffer->data, syn_start.offset, 
                                      syn_examine_text, syn_examine_length);
    syn_examine_text[syn_examine_length] = '\0';

    SyntaxMatches *syn_matches = syn_def->generate_matches(syn_def,
                                                           syn_examine_text,
                                                           syn_examine_length,
                                                           syn_start.offset);

    free(syn_examine_text);

    bv->change_state = bc_get_current_state(&buffer->changes);

    bv->syn_match_cache = (SyntaxMatchCache) {
        .syn_matches = syn_matches,
        .screen_start = *draw_pos
    };
    
    return syn_matches;
}

static int bv_can_use_syntax_match_cache(Buffer *buffer,
                                         const BufferPos *draw_pos)
{
    BufferView *bv = buffer->bv;
    const SyntaxMatchCache *syn_match_cache = &bv->syn_match_cache;

    if (syn_match_cache->syn_matches == NULL ||
        bc_has_state_changed(&buffer->changes,
                             bv->change_state)) {
        return 0;
    }

    Range screen_start_range = {
        .start = syn_match_cache->screen_start,
        .end = syn_match_cache->screen_start
    };

    bf_change_multi_line(buffer, &screen_start_range.start, DIRECTION_UP,
                         SYNTAX_CACHE_LINES, 0);
    bf_change_multi_line(buffer, &screen_start_range.end, DIRECTION_DOWN,
                         SYNTAX_CACHE_LINES, 0);

    if (bf_bp_in_range(&screen_start_range, draw_pos)) {
        return 1;
    }

    return 0;
}

void bv_free_syntax_match_cache(BufferView *bv)
{
    free(bv->syn_match_cache.syn_matches);
    bv->syn_match_cache.syn_matches = NULL;
}

static void bv_set_cell(Cell *cell, size_t offset, size_t col_no,
                        size_t col_width, CellAttribute attr,
                        const char *fmt, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    cell->text_len = vsnprintf(cell->text, CELL_TEXT_LENGTH, fmt, arg_ptr);
    va_end(arg_ptr); 

    cell->text_len = MIN(cell->text_len, CELL_TEXT_LENGTH - 1);
    cell->offset = offset;
    cell->col_no = col_no;
    cell->col_width = col_width;

    if (attr != CA_NONE) {
        cell->attr |= attr;
    }
}

static void bv_populate_buffer_data(const Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    int line_wrap = cf_bool(buffer->config, CV_LINEWRAP);
    BufferPos draw_pos = bv->screen_start;
    size_t buffer_len = bf_length(buffer);
    size_t row = 0;
    size_t col = 0;

    Line *line;
    Cell *cell;
    CharInfo char_info;

    bv_clear_view(bv);

    while (row < bv->rows && draw_pos.offset <= buffer_len) {
        line = &bv->lines[row];

        if (bp_at_line_start(&draw_pos)) {
            line->line_no = draw_pos.line_no;
        }

        if (bv->horizontal_scroll > 0) {
            bp_advance_to_col(&draw_pos, bv->horizontal_scroll);

            if (draw_pos.col_no > bv->horizontal_scroll) {
                bp_prev_char(&draw_pos);
            }
        }

        while (col < bv->cols && draw_pos.offset < buffer_len &&
               !bp_at_line_end(&draw_pos)) {
            cell = &line->cells[col];

            en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, &draw_pos,
                              buffer->config);

            uchar character[CELL_TEXT_LENGTH] = { '\0' };
            gb_get_range(draw_pos.data, draw_pos.offset, (char *)character,
                         char_info.byte_length);

            if (!char_info.is_valid) {
                /* Unicode replacement character */
                bv_set_cell(cell, draw_pos.offset, draw_pos.col_no,
                            1, CA_NONE, "%s", "\xEF\xBF\xBD");
                col++;
            } else if (!char_info.is_printable) {
                char nonprint_draw[] = "^ ";

                if (*character == 127) {
                    nonprint_draw[1] = '?';
                } else {
                    nonprint_draw[1] = character[0] + 64;
                }

                if (!line_wrap && draw_pos.col_no < bv->horizontal_scroll &&
                    (char_info.screen_length + draw_pos.col_no) >
                    bv->horizontal_scroll) {
                    bv_set_cell(cell, draw_pos.offset, draw_pos.col_no + 1,
                                1, CA_NONE, "%c", nonprint_draw[1]);
                } else {
                    bv_set_cell(cell, draw_pos.offset, draw_pos.col_no,
                                1, CA_NONE, "%c", nonprint_draw[0]);

                    if (col == (bv->cols - 1)) {
                        if (line_wrap && row != bv->rows - 1) {
                            line = &bv->lines[++row];
                            cell = &line->cells[(col = 0)];
                            bv_set_cell(cell, draw_pos.offset,
                                        draw_pos.col_no + 1, 1, CA_NONE,
                                        "%c", nonprint_draw[1]);
                        }
                    } else {
                        cell = &line->cells[++col];
                        bv_set_cell(cell, draw_pos.offset, draw_pos.col_no + 1,
                                    1, CA_NONE, "%c", nonprint_draw[1]);
                    }
                }

                col++;
            } else if (*character == '\t') {
                size_t screen_length = char_info.screen_length;
                size_t col_no = draw_pos.col_no;

                if (!line_wrap && draw_pos.col_no < bv->horizontal_scroll &&
                    (char_info.screen_length + draw_pos.col_no) >
                    bv->horizontal_scroll) {
                    screen_length -= (bv->horizontal_scroll - draw_pos.col_no);
                    col_no += (bv->horizontal_scroll - draw_pos.col_no);
                }

                while (screen_length > 0) {
                    size_t line_remaining = MIN(bv->cols - col, screen_length);
                   
                    while (line_remaining > 0) {
                        cell = &line->cells[col++];
                        bv_set_cell(cell, draw_pos.offset, col_no++,
                                    1, CA_NONE, "%c", ' ');
                        screen_length--;
                        line_remaining--;
                    }

                    if (screen_length > 0) {
                        if (line_wrap && row != bv->rows - 1) {
                            line = &bv->lines[++row];
                            cell = &line->cells[(col = 0)];
                        } else {
                            break;
                        }
                    }
                }
            } else {
                size_t line_remaining = bv->cols - col;

                if (!line_wrap && draw_pos.col_no < bv->horizontal_scroll &&
                    (char_info.screen_length + draw_pos.col_no) >
                    bv->horizontal_scroll) {
                    size_t screen_length = char_info.screen_length -
                        (bv->horizontal_scroll - draw_pos.col_no);
                    size_t col_no = draw_pos.col_no + 
                        (bv->horizontal_scroll - draw_pos.col_no);

                    while (col < bv->cols && col < screen_length) {
                        cell = &line->cells[col++];
                        /* Unicode horizontal ellipsis character */
                        bv_set_cell(cell, -1, col_no++, 1, CA_WRAP,
                                    "%s", "\xE2\x80\xA6");
                    }
                } else if (line_remaining < char_info.screen_length) {
                    bv_set_cell(cell, -1, 0, 1, CA_WRAP,
                                "%s", "\xE2\x80\xA6");

                    if ((!line_wrap || row == (bv->rows - 1)) &&
                        draw_pos.offset == buffer->pos.offset) {
                        cell->offset = buffer->pos.offset;
                    }

                    if (line_wrap) {
                        if (row != bv->rows - 1) {
                            line = &bv->lines[++row];
                            col = 0;
                            continue;
                        }
                    }

                    break;
                } else {
                    memcpy(cell->text, character, char_info.byte_length);
                    assert(char_info.byte_length < CELL_TEXT_LENGTH);
                    cell->text[char_info.byte_length] = '\0';
                    cell->col_width = char_info.screen_length;
                    cell->text_len = char_info.byte_length;
                    cell->offset = draw_pos.offset;
                    cell->col_no = draw_pos.col_no;
                    col += char_info.screen_length;
                }
            }

            draw_pos.offset += char_info.byte_length;
            draw_pos.col_no += char_info.screen_length;
        }

        if (bp_at_line_end(&draw_pos) && col < bv->cols) {
            size_t col_no = MAX(draw_pos.col_no, bv->horizontal_scroll);

            if (draw_pos.col_no >= col_no) {
                cell = &line->cells[col++];
                bv_set_cell(cell, draw_pos.offset, col_no++,
                            1, CA_NONE | CA_NEW_LINE, "%c", ' ');
            }

            while (col < bv->cols) {
                cell = &line->cells[col++];
                bv_set_cell(cell, -1, col_no++, 1, CA_NONE | CA_LINE_END,
                            "%c", ' ');
            }

            if (draw_pos.offset == buffer_len) {
                draw_pos.offset++;
            } else {
                bp_next_line(&draw_pos);
            }
        } else if (!line_wrap && col > 0 && col <= bv->cols) {
            if (!bp_next_line(&draw_pos)) {
                row++;
                break;
            }
        }

        col = 0;
        row++;
    }

    bv->rows_drawn = row;

    while (row < bv->rows) {
        line = &bv->lines[row++];
        cell = &line->cells[0];
        bv_set_cell(cell, -1, 0, 1, CA_BUFFER_END, "%c", '~');
    }

    bv->resized = 0;
}

static void bv_clear_view(BufferView *bv)
{
    Line *line;

    for (size_t row = 0; row < bv->rows; row++) {
        line = &bv->lines[row];
        line->line_no = 0;
        memset(line->cells, 0, bv->cols * sizeof(Cell));
    }
}

static void bv_populate_syntax_data(const Session *sess, Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    const BufferPos *draw_pos = &bv->screen_start;
    SyntaxMatches *syn_matches = bv_get_syntax_matches(sess, buffer,
                                                       draw_pos);

    if (syn_matches == NULL || syn_matches->match_num == 0) {
        return;
    }

    Line *line;
    Cell *cell;
    const SyntaxMatch *syn_match;

    for (size_t row = 0; row < bv->rows_drawn; row++) {
        line = &bv->lines[row];

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (cell->text_len == 0 || cell->offset == (size_t)-1) {
                continue;
            }

            syn_match = sy_get_syntax_match(syn_matches, cell->offset);

            if (syn_match != NULL) {
                cell->token = syn_match->token;
            }
        }
    }
}

static void bv_populate_selection_data(Buffer *buffer)
{
    Range select_range;

    if (!bf_get_range(buffer, &select_range)) {
        return;
    }

    BufferView *bv = buffer->bv;
    Line *line;
    Cell *cell;

    for (size_t row = 0; row < bv->rows_drawn; row++) {
        line = &bv->lines[row];

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (cell->text_len == 0) {
                continue;
            }

            if (bf_offset_in_range(&select_range, cell->offset)) {
                cell->attr |= CA_SELECTION;
            }
        }
    }
}

static void bv_populate_colorcolumn_data(Buffer *buffer)
{
    const size_t color_column = cf_int(buffer->config, CV_COLORCOLUMN);

    if (color_column == 0) {
        return;
    }

    BufferView *bv = buffer->bv;
    Line *line;
    Cell *cell;

    for (size_t row = 0; row < bv->rows_drawn; row++) {
        line = &bv->lines[row];

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (cell->col_no == color_column) {
                cell->attr |= CA_COLORCOLUMN;
                break;
            }
        }
    }
}

static void bv_populate_cursor_data(Buffer *buffer)
{
    BufferView *bv = buffer->bv;
    const BufferPos *pos = &buffer->pos;
    Line *line;
    Cell *cell;

    for (size_t row = 0; row < bv->rows_drawn; row++) {
        line = &bv->lines[row];

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (cell->offset == pos->offset) {
                cell->attr |= CA_CURSOR;
                return;
            }
        }
    }

    assert(!"Unable to set cusor in BufferView");
}

int bv_resize(BufferView *bv, size_t rows, size_t cols)
{
    size_t orig_rows_allocated = bv->rows_allocated;

    if (bv->rows != rows) {
        if (bv->rows > rows) {
            bv->rows = rows;
        } else {
            if (bv->rows_allocated < rows) {
                void *ptr = realloc(bv->lines, rows * sizeof(Line));

                if (ptr == NULL) {
                    return 0;
                }

                bv->lines = ptr;
                Line *line;

                for (size_t row = bv->rows_allocated; row < rows; row++) {
                    line = &bv->lines[row];
                    line->line_no = 0;
                    line->cells = calloc(cols, sizeof(Cell));

                    if (line->cells == NULL) {
                        return 0;
                    }
                }

                bv->rows = bv->rows_allocated = rows;
            } else {
                bv->rows = rows;
            }
        }

        bv->resized = 1;
    }

    if (bv->cols != cols) {
        if (bv->cols > cols) {
            bv->cols = cols;
        } else {
            if (bv->cols_allocated < cols) {
                Line *line;

                for (size_t row = 0; row < orig_rows_allocated; row++) {
                    line = &bv->lines[row];

                    void *ptr = realloc(line->cells, cols * sizeof(Cell));

                    if (ptr == NULL) {
                        return 0;
                    }

                    line->cells = ptr;
                    memset(line->cells + bv->cols_allocated, 0,
                           (cols - bv->cols_allocated) * sizeof(Cell));
                }

                bv->cols = bv->cols_allocated = cols;
            } else {
                bv->cols = cols;
            }
        }

        bv->resized = 1;
    }

    return 1;
}

/* Convert column number into screen column number */
size_t bv_screen_col_no(const Buffer *buffer, const BufferPos *pos)
{
    const BufferView *bv = buffer->bv;
    size_t col_no;

    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        col_no = ((pos->col_no - 1) % bv->cols) + 1;
    } else {
        col_no = pos->col_no;
    }

    return col_no;
}

void bv_apply_cell_attributes(BufferView *bv, CellAttribute attr,
                              CellAttribute exclude_cell_attr)
{
    Line *line;
    Cell *cell;

    for (size_t row = 0; row < bv->rows_drawn; row++) {
        line = &bv->lines[row];

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (!(cell->attr & exclude_cell_attr)) {
                cell->attr |= attr;
            }
        }
    }
}
