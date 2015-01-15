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
/* TODO make this configurable
 * this is duplicated in encoding.c */
#define WED_TAB_SIZE 8

static WINDOW *menu;
static WINDOW *status;
static WINDOW *text;
static WINDOW *windows[WINDOW_NUM];
static size_t text_y;
static size_t text_x;

static void draw_prompt(Session *);
static void draw_menu(Session *);
static void draw_status(Session *);
static void draw_buffer(Buffer *, int);
static size_t draw_line(Buffer *, BufferPos, int, int, Range, int, WindowInfo);
static void position_cursor(Buffer *, int);
static void vertical_scroll(Buffer *);
static void vertical_scroll_linewrap(Buffer *);
static void horizontal_scroll(Buffer *);

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
    delwin(menu);
    delwin(text);
    delwin(status);
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

/* Update the menu, status and active buffer views.
 * This is called after a change has been made that
 * needs to be reflected in the UI. */
void update_display(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    int line_wrap = config_bool("linewrap");
    WINDOW *draw_win = windows[buffer->win_info.win_index];

    werase(draw_win);

    draw_menu(sess);

    if (cmd_buffer_active(sess)) {
        draw_prompt(sess);
    } else {
        draw_status(sess);
    }

    if (line_wrap) {
        vertical_scroll_linewrap(buffer);
    } else {
        vertical_scroll(buffer);
        horizontal_scroll(buffer);
    }

    draw_buffer(buffer, line_wrap);

    position_cursor(buffer, line_wrap);

    doupdate();
}

/* Draw top menu */
/* TODO Show other buffers with numbers and colors */
void draw_menu(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    wbkgd(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    wattron(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    mvwprintw(menu, 0, 0, " %s", buffer->file_info.file_name); 
    wclrtoeol(menu);
    wattroff(menu, COLOR_PAIR(MENU_COLOR_PAIR));
    wnoutrefresh(menu); 
}

/* TODO There's a lot more this function could show.
 * The layout needs to be considered. */
void draw_status(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    BufferPos pos = buffer->pos;

    wmove(status, 0, 0);
    wbkgd(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wattron(status, COLOR_PAIR(STATUS_COLOR_PAIR));
    wprintw(status, "Line %zu Column %zu", pos.line_no, pos.col_no); 
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

static void draw_buffer(Buffer *buffer, int line_wrap)
{
    Range select_range;
    int is_selection = get_selection_range(buffer, &select_range);
    size_t line_num = buffer->win_info.height;
    size_t line_count = 0;
    BufferPos draw_pos = buffer->screen_start;
    WINDOW *draw_win = windows[buffer->win_info.win_index];

    while (line_count < line_num && draw_pos.line != NULL) {
        line_count += draw_line(buffer, draw_pos, line_count, is_selection, 
                                select_range, line_wrap, buffer->win_info);

        draw_pos.line = draw_pos.line->next;
        draw_pos.line_no++;

        if (line_wrap) {
            draw_pos.offset = 0;
            draw_pos.col_no = 1;
        }
    }

    wstandend(draw_win);

    while (line_count < buffer->win_info.height) {
        mvwaddch(text, buffer->win_info.start_y + line_count++, buffer->win_info.start_x, '~');
    }
}

static size_t draw_line(Buffer *buffer, BufferPos draw_pos, int y, int is_selection, 
                        Range select_range, int line_wrap, WindowInfo win_info)
{
    Line *line = draw_pos.line;

    if (line->length == 0) {
        return 1;
    }

    CharInfo char_info;

    if (!line_wrap) {
        if (line->screen_length < draw_pos.col_no) {
            return 1;
        }

        size_t col_no = 1;
        draw_pos.offset = 0;

        while (col_no < draw_pos.col_no && draw_pos.offset < line->length) {
            buffer->cef.char_info(&char_info, CIP_DEFAULT, draw_pos);
            draw_pos.offset += char_info.byte_length;
            col_no++;
        }

        if (col_no < draw_pos.col_no) {
            return 1;
        }
    }

    WINDOW *draw_win = windows[win_info.win_index];
    size_t scr_line_num = 0;
    size_t start_col = draw_pos.col_no;
    size_t window_width = win_info.start_x + win_info.width;

    while (draw_pos.offset < line->length && scr_line_num < win_info.height) {
        wmove(draw_win, win_info.start_y + y++, win_info.start_x);
        scr_line_num++;

        for (size_t k = win_info.start_x; k < window_width && draw_pos.offset < line->length; k++) {
            if (is_selection && bufferpos_in_range(select_range, draw_pos)) {
                wattron(draw_win, A_REVERSE);
            } else {
                wattroff(draw_win, A_REVERSE);
            }

            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, draw_pos);
            waddnstr(draw_win, line->text + draw_pos.offset, char_info.byte_length);

            draw_pos.col_no += char_info.screen_length;
            draw_pos.offset += char_info.byte_length;
        }

        if (!line_wrap) {
            break;
        }
    }

    if (scr_line_num < screen_height_from_screen_length(win_info, line->screen_length - start_col - 1)) {
        scr_line_num++;
    }

    return scr_line_num;
}

static void position_cursor(Buffer *buffer, int line_wrap)
{
    WindowInfo win_info = buffer->win_info;
    WINDOW *draw_win = windows[win_info.win_index];
    BufferPos pos = buffer->pos;
    BufferPos screen_start = buffer->screen_start;
    size_t cursor_y = 0, cursor_x;

    if (line_wrap) {
        if (screen_start.line_no < pos.line_no && screen_start.col_no > 1) {
            size_t screen_length = line_screen_length(buffer, screen_start, screen_start.line->length); 
            cursor_y += screen_height_from_screen_length(win_info, screen_length);
            screen_start.line = screen_start.line->next;
            screen_start.line_no++;
            screen_start.offset = 0;
            screen_start.col_no = 1;
        }

        while (screen_start.line_no < pos.line_no) {
            cursor_y += line_screen_height(win_info, screen_start.line);
            screen_start.line = screen_start.line->next;
            screen_start.line_no++;
            screen_start.offset = 0;
            screen_start.col_no = 1;
        }

        size_t screen_length = pos.col_no - screen_start.col_no;
        cursor_y += win_info.start_y + screen_height_from_screen_length(win_info, screen_length) - 1;
        cursor_x = win_info.start_x + (screen_length %= win_info.width);
    } else {
        cursor_y = win_info.start_y + pos.line_no - screen_start.line_no;
        cursor_x = win_info.start_x + pos.col_no - screen_start.col_no;
    }

    wmove(draw_win, cursor_y, cursor_x);
    wnoutrefresh(draw_win);
}

/* TODO This isn't generic, it assumes line wrapping is enabled. This 
 * may not be the case in future when horizontal scrolling is added. */
size_t screen_col_no(WindowInfo win_info, BufferPos pos)
{
    size_t col_no;

    if (config_bool("linewrap")) {
        col_no = ((pos.col_no - 1) % win_info.width) + 1;
    } else {
        col_no = pos.col_no;
    }

    return col_no;
}

/* The number of screen columns taken up by this line segment */
size_t line_screen_length(Buffer *buffer, BufferPos pos, size_t limit_offset)
{
    /*if (pos.line->length == pos.offset) {
        return 1;
    } else*/ if (limit_offset <= pos.offset) {
        return 0;
    }

    size_t screen_length = 0;
    limit_offset = (limit_offset > pos.line->length ? pos.line->length : limit_offset);
    CharInfo char_info;

    while (pos.offset < limit_offset) {
        buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, pos); 
        screen_length += char_info.screen_length;
        pos.col_no += char_info.screen_length;
        pos.offset += char_info.byte_length;
    }

    return screen_length;
}

size_t line_screen_height(WindowInfo win_info, Line *line)
{
    return screen_height_from_screen_length(win_info, line->screen_length);
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

/* TODO consider using ncurses scroll function as well */
/* Determine if the screen needs to be scrolled and what parts need to be updated if so */
static void vertical_scroll(Buffer *buffer)
{
    WindowInfo win_info = buffer->win_info;
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &buffer->screen_start;

    if (pos.line_no < screen_start->line_no) {
        screen_start->line = pos.line;
        screen_start->line_no = pos.line_no;
    } else {
        size_t diff = pos.line_no - screen_start->line_no;

        if (diff < win_info.height) {
            return;
        }

        diff -= (win_info.height - 1);

        if (diff > win_info.height) {
            screen_start->line = pos.line;
            screen_start->line_no = pos.line_no;
        } else {
            size_t start_col = screen_start->col_no;
            pos_change_multi_line(buffer, screen_start, DIRECTION_DOWN, diff, 0);
            /* Preserve horizontal scroll */
            screen_start->col_no = start_col;
        }
    }
}

static void vertical_scroll_linewrap(Buffer *buffer)
{
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &buffer->screen_start;

    if ((pos.line_no < screen_start->line_no) ||
        (pos.line_no == screen_start->line_no && pos.col_no < screen_start->col_no)) {
        *screen_start = pos;

        if (!bufferpos_at_screen_line_start(*screen_start, buffer->win_info)) {
            bpos_to_screen_line_start(buffer, screen_start, 0, 0);
        }
    } else {
        BufferPos start = pos;

        if (!bufferpos_at_screen_line_start(start, buffer->win_info)) {
            bpos_to_screen_line_start(buffer, &start, 0, 0);
        }

        size_t line_num = buffer->win_info.height;

        while (bufferpos_compare(start, *screen_start) != 0 && --line_num > 0) {
            pos_change_line(buffer, &start, DIRECTION_UP, 0);
        }

        if (line_num == 0) {
            *screen_start = start;
        }
    }
}

static void horizontal_scroll(Buffer *buffer)
{
    size_t diff;
    Direction direction;
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &buffer->screen_start;

    if (pos.col_no > screen_start->col_no) {
        diff = pos.col_no - screen_start->col_no;
        direction = DIRECTION_RIGHT;
    } else {
        diff = screen_start->col_no - pos.col_no;
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

        screen_start->col_no += diff;
    } else {
        screen_start->col_no -= diff;
    }
}
