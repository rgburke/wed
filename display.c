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

#include <string.h>
#include <ncurses.h>
#include "display.h"
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "config.h"

#define WINDOW_NUM 3
#define WED_TAB_SIZE 8

static WINDOW *menu;
static WINDOW *status;
static WINDOW *text;
static WINDOW *windows[WINDOW_NUM];
static size_t text_y;
static size_t text_x;

static void draw_prompt(Session *);
static void handle_draw_status(Line *, LineDrawStatus *);
static size_t draw_line(Line *, size_t, int, LineDrawStatus *, int, Range, int, WindowInfo);
static void convert_pos_to_point(Point *, WindowInfo, BufferPos);
static void vertical_scroll(Buffer *, Point *, Point);
static void horizontal_scroll(Buffer *buffer, Point *screen_start, Point cursor);

/* ncurses setup */
void init_display(void)
{
    initscr();

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(MENU_COLOR_PAIR, COLOR_BLUE, COLOR_WHITE);
        init_pair(TAB_COLOR_PAIR, COLOR_BLUE, COLOR_WHITE);
        init_pair(STATUS_COLOR_PAIR, COLOR_YELLOW, COLOR_BLUE);
    }

    raw();
    noecho();
    nl();
    keypad(stdscr, TRUE);
    curs_set(1);
    set_tabsize(WED_TAB_SIZE);

    text_y = LINES - 2;
    text_x = COLS;

    windows[0] = menu = newwin(1, COLS, 0, 0); 
    windows[1] = text = newwin(text_y, text_x, 1, 0);
    windows[2] = status = newwin(1, COLS, LINES - 1, 0);

    refresh();
}

void end_display(void)
{
    endwin();
}

void init_all_window_info(Session *sess)
{
    Buffer *buffer = sess->buffers;

    while (buffer != NULL) {
        init_window_info(&buffer->win_info);
        buffer = buffer->next;
    }

    init_window_info(&sess->cmd_prompt.cmd_buffer->win_info);
    sess->cmd_prompt.cmd_buffer->win_info.height = 1;
    sess->cmd_prompt.cmd_buffer->win_info.win_index = WIN_STATUS;
}

void init_window_info(WindowInfo *win_info)
{
    win_info->height = text_y;
    win_info->width = text_x;
    win_info->start_y = 0;
    win_info->start_x = 0;
    win_info->win_index = WIN_TEXT;
}

void refresh_display(Session *sess)
{
    draw_menu(sess);
    draw_status(sess);
    draw_buffer(sess, DRAW_LINE_FULL_REFRESH, config_bool("linewrap"));
    update_display(sess);
}

/* Draw top menu */
/* TODO Show other buffers with numbers and colors */
void draw_menu(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    wclrtoeol(menu);
    wbkgd(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    wattron(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    mvwprintw(menu, 0, 0, " %s", buffer->file_info.file_name); 
    wattroff(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    wnoutrefresh(menu); 
}

/* TODO There's a lot more this function could show.
 * The layout needs to be considered. */
void draw_status(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    size_t line_no = get_pos_line_number(buffer);
    size_t col_no = get_pos_col_number(buffer);

    wmove(status, 0, 0);
    wbkgd(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wattron(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wprintw(status, "Line %zu Column %zu", line_no, col_no); 
    wclrtoeol(status);
    wattroff(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wnoutrefresh(status); 
}

static void draw_prompt(Session *sess)
{
    wmove(status, 0, 0);
    wbkgd(status, COLOR_PAIR(0));
    wattron(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wprintw(status, "%s:", sess->cmd_prompt.cmd_text); 
    wattroff(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wprintw(status, " "); 

    if (buffer_byte_num(sess->cmd_prompt.cmd_buffer) == 0) {
        wclrtoeol(status);
    }

    size_t prompt_size = strlen(sess->cmd_prompt.cmd_text) + 2;
    WindowInfo *win_info = &sess->cmd_prompt.cmd_buffer->win_info;
    win_info->start_x = prompt_size;
    win_info->width = text_x - prompt_size;
}

static void handle_draw_status(Line *line, LineDrawStatus *draw_status)
{
    if (line->is_dirty) {
        if (line->is_dirty & DRAW_LINE_REFRESH_DOWN) {
            *draw_status |= DRAW_LINE_REFRESH_DOWN;
        } else if (line->is_dirty & DRAW_LINE_END_REFRESH_DOWN) {
            *draw_status &= ~DRAW_LINE_REFRESH_DOWN;
        } else if (line->is_dirty & DRAW_LINE_SCROLL_REFRESH_DOWN) {
            *draw_status |= DRAW_LINE_SCROLL_REFRESH_DOWN;
        } else if (line->is_dirty & DRAW_LINE_SCROLL_END_REFRESH_DOWN) {
            *draw_status &= ~DRAW_LINE_SCROLL_REFRESH_DOWN;
        }    
    }
}

/* Refresh the active buffer on the screen. Only draw
 * necessary buffer parts. */
void draw_buffer(Session *sess, LineDrawStatus draw_status, int line_wrap)
{
    Buffer *buffer = sess->active_buffer;
    WINDOW *draw_win = windows[buffer->win_info.win_index];

    if (draw_status & DRAW_LINE_FULL_REFRESH) {
        wclear(draw_win);
    }

    Range select_range;
    int is_selection = get_selection_range(buffer, &select_range);
    size_t line_num = buffer->win_info.height;
    size_t line_count = 0;
    BufferPos screen_start = buffer->screen_start;
    size_t horizontal_offset = screen_start.offset;
    Line *line = buffer->lines;

    while (line != screen_start.line) {
        if (line->is_dirty) {
            handle_draw_status(line, &draw_status);
            line->is_dirty = DRAW_LINE_NO_CHANGE;
        }

        line = line->next;
    }

    line_count += draw_line(line, horizontal_offset, line_count, &draw_status,
                            is_selection, select_range, line_wrap, buffer->win_info);

    handle_draw_status(line, &draw_status);
    line->is_dirty = DRAW_LINE_NO_CHANGE;
    line = line->next;

    if (line_wrap) {
        horizontal_offset = 0;
    }

    while (line_count < line_num && line != NULL) {
        line_count += draw_line(line, horizontal_offset, line_count, &draw_status,
                                is_selection, select_range, line_wrap, buffer->win_info);

        handle_draw_status(line, &draw_status);
        line->is_dirty = DRAW_LINE_NO_CHANGE;
        line = line->next;
    }

    if (draw_status) {
        if (!line_wrap && horizontal_offset > 0 && 
            (line_count < buffer->win_info.height)) {

            wmove(text, buffer->win_info.start_y + line_count, buffer->win_info.start_x);
            wclrtobot(draw_win);
        } else {
            wstandend(draw_win);

            while (line_count < buffer->win_info.height) {
                mvwaddch(text, buffer->win_info.start_y + line_count++, buffer->win_info.start_x, '~');
                wclrtoeol(draw_win);
            }
        }
    }

    while (line != NULL) {
        if (line->is_dirty) {
            line->is_dirty = DRAW_LINE_NO_CHANGE;
        }

        line = line->next;
    }
}

static size_t draw_line(Line *line, size_t char_index, int y, LineDrawStatus *draw_status, 
                        int is_selection, Range select_range, int line_wrap, WindowInfo win_info)
{
    WINDOW *draw_win = windows[win_info.win_index];

    if (!(*draw_status || line->is_dirty)) {
        if (char_index > 0) {
            return line_offset_screen_height(win_info, line, char_index, line->length);
        } 

        return line_screen_height(win_info, line);
    }

    if (line->length == 0) {
        if (*draw_status || (line->is_dirty & (DRAW_LINE_SHRUNK | DRAW_LINE_REFRESH_DOWN))) {
            wmove(draw_win, win_info.start_y + y, win_info.start_x);
            wclrtoeol(draw_win);
        }

        return 1;
    }

    if (!line_wrap) {
        size_t col_no = 0;
        size_t index = 0;
        
        while (col_no < char_index && index < line->length) {
            index += char_byte_length(line->text[index]);
            col_no++;
        }

        if (col_no < char_index) {
            return 1;
        } else {
            char_index = index;
        }
    }

    BufferPos draw_pos = { .line = line, .offset = char_index };
    size_t scr_line_num = 0;
    size_t start_index = char_index;
    size_t char_byte_len;
    size_t window_width = win_info.start_x + win_info.width;

    while (draw_pos.offset < line->length && scr_line_num < win_info.height) {
        wmove(draw_win, win_info.start_y + y++, win_info.start_x);
        scr_line_num++;

        for (size_t k = win_info.start_x; 
             k < window_width && draw_pos.offset < line->length;
             draw_pos.offset += char_byte_len, k++) {

            if (is_selection && bufferpos_in_range(select_range, draw_pos)) {
                wattron(draw_win, A_REVERSE);
            } else {
                wattroff(draw_win, A_REVERSE);
            }

            char_byte_len = char_byte_length(line->text[draw_pos.offset]);
            waddnstr(draw_win, line->text + draw_pos.offset, char_byte_len);
        }

        if (!line_wrap) {
            break;
        }
    }

    if (*draw_status || (line->is_dirty & (DRAW_LINE_SHRUNK | DRAW_LINE_REFRESH_DOWN))) {
        wclrtoeol(draw_win);  
    }

    if (scr_line_num < line_offset_screen_height(win_info, line, start_index, line->length)) {
        wmove(draw_win, win_info.start_y + y, win_info.start_x);
        wclrtoeol(draw_win);
        scr_line_num++;
    }

    return scr_line_num;
}

/* Update the menu, status and active buffer views.
 * This is called after a change has been made that
 * needs to be reflected in the UI. */
void update_display(Session *sess)
{
    if (cmd_buffer_active(sess)) {
        draw_prompt(sess);
    } else {
        draw_status(sess);
    }

    Buffer *buffer = sess->active_buffer;
    WindowInfo win_info = buffer->win_info;
    int line_wrap = config_bool("linewrap");
    WINDOW *draw_win = windows[buffer->win_info.win_index];

    Point screen_start, cursor;
    convert_pos_to_point(&screen_start, win_info, buffer->screen_start);
    convert_pos_to_point(&cursor, win_info, buffer->pos);

    vertical_scroll(buffer, &screen_start, cursor);

    if (!line_wrap) {
        screen_start.col_no = buffer->screen_start.offset;
        horizontal_scroll(buffer, &screen_start, cursor);
    }

    draw_buffer(sess, 0, line_wrap);

    size_t cursor_y = buffer->win_info.start_y + cursor.line_no - screen_start.line_no;
    size_t cursor_x = buffer->win_info.start_x + cursor.col_no - screen_start.col_no;
    wmove(draw_win, cursor_y, cursor_x);
    wnoutrefresh(draw_win);

    doupdate();
}

static void convert_pos_to_point(Point *point, WindowInfo win_info, BufferPos pos)
{
    point->line_no = screen_line_no(win_info, pos);
    point->col_no = screen_col_no(win_info, pos);
}

/* Calculate the line number pos represents counting 
 * wrapped lines as whole lines */
size_t screen_line_no(WindowInfo win_info, BufferPos pos)
{
    size_t line_no = 0;
    Line *line = pos.line;

    line_no += line_pos_screen_height(win_info, pos);

    while ((line = line->prev) != NULL) {
        line_no += line_screen_height(win_info, line);
    }

    return line_no;
}

/* TODO This isn't generic, it assumes line wrapping is enabled. This 
 * may not be the case in future when horizontal scrolling is added. */
size_t screen_col_no(WindowInfo win_info, BufferPos pos)
{
    size_t screen_length = line_screen_length(pos.line, 0, pos.offset);

    if (config_bool("linewrap")) {
        screen_length %= win_info.width;
    }

    return screen_length; 
}

/* The number of screen columns taken up by this line segment */
size_t line_screen_length(Line *line, size_t start_offset, size_t limit_offset)
{
    size_t screen_length = 0;

    if (line == NULL || (limit_offset <= start_offset)) {
        return screen_length;
    }

    for (size_t k = start_offset; k < line->length && k < limit_offset; k++) {
        screen_length += byte_screen_length(line->text[k], line, k);
    }

    return screen_length;
}

size_t line_screen_height(WindowInfo win_info, Line *line)
{
    return screen_height_from_screen_length(win_info, line->screen_length);
}

size_t line_pos_screen_height(WindowInfo win_info, BufferPos pos)
{
    size_t screen_length = line_screen_length(pos.line, 0, pos.offset);
    return screen_height_from_screen_length(win_info, screen_length);
}

size_t line_offset_screen_height(WindowInfo win_info, Line *line, size_t start_offset, size_t limit_offset)
{
    size_t screen_length = line_screen_length(line, start_offset, limit_offset);
    return screen_height_from_screen_length(win_info, screen_length);
}

/* This calculates the number of lines that text, when displayed on the screen
 * takes up screen_length columns, takes up */
size_t screen_height_from_screen_length(WindowInfo win_info, size_t screen_length)
{
    if (!config_bool("linewrap") || screen_length == 0) {
        return 1;
    } else if ((screen_length % win_info.width) == 0) {
        screen_length++;
    }

    return roundup_div(screen_length, win_info.width);
}

/* The number of columns a byte takes up on the screen.
 * Continuation bytes take up no screen space for example. */
size_t byte_screen_length(char c, Line *line, size_t offset)
{
    if (line->length == offset) {
        return 1;
    } else if (c == '\t') {
        if (offset == 0) {
            return WED_TAB_SIZE;
        }

        size_t col_index = line_screen_length(line, 0, offset);
        return WED_TAB_SIZE - (col_index % WED_TAB_SIZE);
    }

    return ((c & 0xc0) != 0x80);
}

/* The length in bytes of a character 
 * (assumes we're on the first byte of the character) */
size_t char_byte_length(char c)
{
    if (!(c & 0x80)) {
        return 1;
    }

    if ((c & 0xFC) == 0xFC) {
        return 6;
    }

    if ((c & 0xF8) == 0xF8) {
        return 5;
    }

    if ((c & 0xF0) == 0xF0) {
        return 4;
    }

    if ((c & 0xE0) == 0xE0) {
        return 3;
    }

    if ((c & 0xC0) == 0xC0) {
        return 2;
    }

    return 0;
}

/* Determine if the screen needs to be scrolled and what parts need to be updated if so */
static void vertical_scroll(Buffer *buffer, Point *screen_start, Point cursor)
{
    size_t diff;
    Direction direction;

    if (cursor.line_no > screen_start->line_no) {
        diff = cursor.line_no - screen_start->line_no;
        direction = DIRECTION_DOWN;
    } else {
        diff = screen_start->line_no - cursor.line_no;
        direction = DIRECTION_UP;
    }

    if (diff == 0) {
        return;
    }

    WindowInfo win_info = buffer->win_info;

    if (direction == DIRECTION_DOWN) {
        if (diff < win_info.height) {
            return;
        }

        diff -= (win_info.height - 1);

        size_t draw_start = diff % win_info.height;

        if (draw_start == 0) {
            draw_start = win_info.height;
        }

        Line *line = get_line_from_offset(buffer->pos.line, DIRECTION_UP, draw_start - 1);
        line->is_dirty |= DRAW_LINE_SCROLL_REFRESH_DOWN;
    }

    pos_change_multi_line(buffer, &buffer->screen_start, direction, diff, 0);
    convert_pos_to_point(screen_start, buffer->win_info, buffer->screen_start);

    if (direction == DIRECTION_UP) {
        buffer->screen_start.line->is_dirty |= DRAW_LINE_SCROLL_REFRESH_DOWN;
        Line *line = get_line_from_offset(buffer->screen_start.line, DIRECTION_DOWN, diff);
        line->is_dirty |= DRAW_LINE_SCROLL_END_REFRESH_DOWN;
    }

    scrollok(text, TRUE);
    wscrl(text, diff * DIRECTION_OFFSET(direction));
    scrollok(text, FALSE);
}

static void horizontal_scroll(Buffer *buffer, Point *screen_start, Point cursor)
{
    size_t diff;
    Direction direction;

    if (cursor.col_no > screen_start->col_no) {
        diff = cursor.col_no - screen_start->col_no;
        direction = DIRECTION_RIGHT;
    } else {
        diff = screen_start->col_no - cursor.col_no;
        direction = DIRECTION_LEFT;
    }

    if (diff == 0) {
        return;
    }

    WindowInfo win_info = buffer->win_info;

    if (direction == DIRECTION_RIGHT) {
        if (diff < win_info.width) {
            return;
        }

        diff -= (win_info.width - 1);

        buffer->screen_start.offset += diff;
    } else {
        buffer->screen_start.offset -= diff;
    }

    screen_start->col_no = buffer->screen_start.offset;
    
    buffer->screen_start.line->is_dirty |= DRAW_LINE_REFRESH_DOWN;
}
