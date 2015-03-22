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
#include <sys/ioctl.h>
#include <unistd.h>
#include <ncurses.h>
#include "display.h"
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "config.h"

#define WINDOW_NUM 4
#define STATUS_TEXT_SIZE 512
#define MAX_MENU_BUFFER_WIDTH 30

typedef enum {
    CP_MENU = 1,
    CP_STATUS,
    CP_ERROR,
    CP_LINE_NO,
    CP_BUFFER_END,
    CP_ACTIVE_BUFFER
} COLOUR_PAIRS;

static WINDOW *menu;
static WINDOW *status;
static WINDOW *text;
static WINDOW *lineno;
static WINDOW *windows[WINDOW_NUM];
static size_t text_y = 0;
static size_t text_x = 0;
static size_t line_no_x = 0;

static void draw_prompt(Session *);
static void draw_menu(Session *);
static void draw_status(Session *);
static size_t draw_status_file_info(Session *, size_t);
static size_t draw_status_pos_info(Session *, size_t);
static void draw_status_general_info(Session *, size_t, size_t);
static void draw_buffer(Buffer *, int);
static size_t draw_line(Buffer *, BufferPos, int, int, Range, int, WindowInfo);
static void draw_char(CharInfo, BufferPos, WINDOW *, size_t, int);
static void position_cursor(Buffer *, int);
static void vertical_scroll(Buffer *);
static void vertical_scroll_linewrap(Buffer *);
static void horizontal_scroll(Buffer *);
static size_t update_line_no_width(Buffer *, int);

/* ncurses setup */
void init_display(void)
{
    initscr();

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_MENU, COLOR_BLUE, COLOR_WHITE);
        init_pair(CP_STATUS, COLOR_YELLOW, COLOR_BLUE);
        init_pair(CP_ERROR, COLOR_WHITE, COLOR_RED);
        init_pair(CP_LINE_NO, COLOR_YELLOW, -1);
        init_pair(CP_BUFFER_END, COLOR_BLUE, -1);
        init_pair(CP_ACTIVE_BUFFER, COLOR_BLUE, -1);
    }

    raw();
    noecho();
    nl();
    keypad(stdscr, TRUE);
    curs_set(1);
    set_tabsize(config_int("tabwidth"));

    text_y = LINES - 2;
    text_x = COLS;

    windows[0] = menu = newwin(1, COLS, 0, 0); 
    windows[1] = text = newwin(text_y, text_x, 1, 0);
    windows[2] = status = newwin(1, COLS, LINES - 1, 0);
    windows[3] = lineno = newwin(0, 0, 1, 0);

    refresh();
}

void resize_display(Session *sess)
{
    struct winsize win_size;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size) == -1) {
        /* TODO handle */
    }

    text_y = win_size.ws_row - 2;
    text_x = win_size.ws_col;

    resizeterm(win_size.ws_row, win_size.ws_col);
    wresize(menu, 1, text_x); 
    wresize(text, text_y, text_x);
    wresize(status, 1, text_x);

    init_all_window_info(sess);
    update_display(sess);
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

    init_window_info(&sess->error_buffer->win_info);
    init_window_info(&sess->cmd_prompt.cmd_buffer->win_info);
    sess->cmd_prompt.cmd_buffer->win_info.height = 1;
    sess->cmd_prompt.cmd_buffer->win_info.draw_window = WIN_STATUS;
}

void init_window_info(WindowInfo *win_info)
{
    win_info->height = text_y;
    win_info->width = text_x;
    win_info->start_y = 0;
    win_info->start_x = 0;
    win_info->line_no_width = 0;
    win_info->draw_window = WIN_TEXT;
}

/* Update the menu, status and active buffer views.
 * This is called after a change has been made that
 * needs to be reflected in the UI. */
void update_display(Session *sess)
{
    Buffer *buffer = sess->active_buffer;
    int line_wrap = config_bool("linewrap");
    WINDOW *draw_win = windows[buffer->win_info.draw_window];

    if (line_wrap) {
        vertical_scroll_linewrap(buffer);
    } else {
        vertical_scroll(buffer);
        horizontal_scroll(buffer);
    }

    if (!cmd_buffer_active(sess)) {
        update_line_no_width(buffer, line_wrap);
    }

    draw_menu(sess);
    werase(draw_win);

    if (cmd_buffer_active(sess)) {
        draw_prompt(sess);
    } else {
        draw_status(sess);
    }

    draw_buffer(buffer, line_wrap);

    position_cursor(buffer, line_wrap);

    doupdate();
}

/* TODO draw_menu and draw_status* need to consider file names with 
 * UTF-8 characters when calculating the screen space they will take.
 * i.e. use wsprintf instead of snprintf */
/* Draw top menu */
void draw_menu(Session *sess)
{
    const char *tab_fmt = " %zu %s ";
    char buffer_display[MAX_MENU_BUFFER_WIDTH];
    Buffer *buffer;
    size_t total_used_space = 0;
    size_t used_space = 0;

    if (sess->active_buffer_index < sess->menu_first_buffer_index) {
        sess->menu_first_buffer_index = sess->active_buffer_index;
    } else {
        buffer = sess->active_buffer;
        size_t start_index = sess->active_buffer_index;

        while (1) {
            used_space = snprintf(buffer_display, MAX_MENU_BUFFER_WIDTH, tab_fmt, start_index + 1, buffer->file_info.file_name);
            used_space = (used_space > MAX_MENU_BUFFER_WIDTH ? MAX_MENU_BUFFER_WIDTH : used_space);

            if ((total_used_space + used_space > text_x) ||
                start_index == 0 || 
                start_index == sess->menu_first_buffer_index) {
                break;
            }

            total_used_space += used_space;
            buffer = get_buffer(sess, --start_index);
        }

        if (total_used_space + used_space > text_x) {
            sess->menu_first_buffer_index = start_index + 1;
        }  

        total_used_space = 0;
        used_space = 0;
    }

    buffer = get_buffer(sess, sess->menu_first_buffer_index);

    werase(menu);
    wbkgd(menu, COLOR_PAIR(CP_MENU));
    wattron(menu, COLOR_PAIR(CP_MENU));

    for (size_t buffer_index = sess->menu_first_buffer_index; 
         buffer_index < sess->buffer_num;
         buffer_index++) {

        used_space = snprintf(buffer_display, MAX_MENU_BUFFER_WIDTH, tab_fmt, buffer_index + 1, buffer->file_info.file_name);
        used_space = (used_space > MAX_MENU_BUFFER_WIDTH ? MAX_MENU_BUFFER_WIDTH : used_space);

        if (total_used_space + used_space > text_x) {
            break;
        }

        if (buffer_index == sess->active_buffer_index) {
            wattron(menu, COLOR_PAIR(CP_ACTIVE_BUFFER));
            mvwprintw(menu, 0, total_used_space, buffer_display); 
            wattroff(menu, COLOR_PAIR(CP_ACTIVE_BUFFER));
        } else {
            mvwprintw(menu, 0, total_used_space, buffer_display); 
        }

        total_used_space += used_space;
        buffer = buffer->next;
    }

    wattroff(menu, COLOR_PAIR(CP_MENU));
    wnoutrefresh(menu); 
}

void draw_status(Session *sess)
{
    int segment_num = 2;

    if (has_msgs(sess)) {
        segment_num = 3;
    }

    size_t max_segment_width = text_x / segment_num;

    werase(status);
    wmove(status, 0, 0);
    wbkgd(status, COLOR_PAIR(CP_STATUS));
    wattron(status, COLOR_PAIR(CP_STATUS));

    size_t file_info_size = draw_status_file_info(sess, max_segment_width);
    size_t file_pos_size = draw_status_pos_info(sess, max_segment_width);

    if (segment_num == 3) {
        size_t available_space = text_x - file_info_size - file_pos_size - 1;
        draw_status_general_info(sess, file_info_size, available_space);
    }

    wattroff(status, COLOR_PAIR(CP_STATUS));
    wnoutrefresh(status); 
}

static size_t draw_status_file_info(Session *sess, size_t max_segment_width)
{
    char status_text[STATUS_TEXT_SIZE];
    size_t file_info_free = max_segment_width;
    FileInfo file_info = sess->active_buffer->file_info;

    char *file_info_text = " ";

    if (!file_exists(file_info)) {
        file_info_text = " [new] ";
    } else if (!can_write_file(file_info)) {
        file_info_text = " [readonly] ";
    }

    file_info_free -= (strlen(file_info_text) + 3);
    size_t file_info_size;
    char *file_path = NULL;

    if (file_exists(file_info)) {
        file_path = file_info.abs_path;
    } else if (has_file_path(file_info)) {
        file_path = file_info.rel_path;
    }

    if (file_path == NULL || strlen(file_path) > file_info_free) {
        file_path = file_info.file_name;
    }
    
    if (strlen(file_path) > file_info_free) {
        int file_char_num = file_info_free - 3;
        file_info_size = snprintf(status_text, max_segment_width, " \"%.*s...\"%s", file_char_num, file_path, file_info_text);
    } else {
        file_info_size = snprintf(status_text, max_segment_width, " \"%s\"%s", file_path, file_info_text);
    }

    wprintw(status, status_text);

    return file_info_size;
}

static size_t draw_status_pos_info(Session *sess, size_t max_segment_width)
{
    char status_text[STATUS_TEXT_SIZE];
    Buffer *buffer = sess->active_buffer;
    BufferPos pos = buffer->pos;
    BufferPos screen_start = buffer->screen_start;
    WindowInfo win_info = buffer->win_info;

    char rel_pos[5];
    memset(rel_pos, 0, sizeof(rel_pos));

    size_t lines_above = screen_start.line_no - 1;
    size_t lines_below;

    if ((screen_start.line_no + win_info.height - 1) >= buffer->line_num) {
        lines_below = 0; 
    } else {
        lines_below = buffer->line_num - (screen_start.line_no + win_info.height - 1);
    }

    if (lines_below == 0) {
        if (lines_above == 0) {
            strcpy(rel_pos, "All");
        } else {
            strcpy(rel_pos, "Bot");
        }
    } else if (lines_above == 0) {
        strcpy(rel_pos, "Top");
    } else {
        double pos_pct = (int)((lines_above / (double)(lines_above + lines_below)) * 100);
        snprintf(rel_pos, 5, "%2d%%%%", (int)pos_pct);
    }

    int pos_info_size = snprintf(status_text, STATUS_TEXT_SIZE, 
                                 "Length: %zu Lines: %zu | Line: %zu Col: %zu | %s ",
                                 buffer->byte_num, buffer->line_num, pos.line_no, pos.col_no,
                                 rel_pos);

    if (pos_info_size < 0 || (size_t)pos_info_size > max_segment_width) {
        pos_info_size = snprintf(status_text, max_segment_width, 
                                 "Line: %zu Col: %zu ", pos.line_no, pos.col_no);
    }

    if (pos_info_size < 0 || (size_t)pos_info_size > max_segment_width) {
        pos_info_size = snprintf(status_text, max_segment_width, 
                                 "L:%zu C:%zu ", pos.line_no, pos.col_no);
    }

    mvwprintw(status, 0, text_x - strlen(status_text) - 1, status_text);

    return pos_info_size;
}

static void draw_status_general_info(Session *sess, size_t file_info_size, size_t available_space)
{
    char status_text[STATUS_TEXT_SIZE];
    available_space -= 3;
    mvwprintw(status, 0, file_info_size - 1, " | ");
    char *msg = join_lines(sess->msg_buffer, ". ");

    if (msg != NULL) {
        size_t msg_length = strlen(msg);

        if (msg_length > available_space) {
            char *fmt = "%%.%ds... (F12 view full) |";
            char msg_fmt[STATUS_TEXT_SIZE];
            msg_length = available_space - strlen(fmt) + 5;
            snprintf(msg_fmt, STATUS_TEXT_SIZE, fmt, msg_length);
            snprintf(status_text, available_space, msg_fmt, msg);
        } else {
            snprintf(status_text, available_space, "%s", msg); 
        }

        mvwprintw(status, 0, file_info_size - 1 + 3, status_text);
        free(msg);
    }

    clear_msgs(sess);
}

void draw_errors(Session *sess)
{
    Buffer *error_buffer = sess->error_buffer;
    Line *line = error_buffer->lines;
    size_t screen_lines = 0;
    WindowInfo *win_info = &error_buffer->win_info;

    while (line != NULL) {
        screen_lines += line_screen_height(*win_info, line);
        line = line->next;
    }
    
    size_t curr_height = win_info->height - win_info->start_y;
    size_t diff;

    if (curr_height > screen_lines) {
        diff = curr_height - screen_lines;
        win_info->start_y += diff;
        win_info->height -= diff;
    } else if (curr_height < screen_lines) {
        diff = screen_lines - curr_height;
        win_info->start_y -= diff;
        win_info->height += diff;
    }

    wattron(text, COLOR_PAIR(CP_ERROR));
    draw_buffer(error_buffer, 1);
    wattroff(text, COLOR_PAIR(CP_ERROR));
    wnoutrefresh(text);

    wmove(status, 0, 0);
    werase(status);
    wbkgd(status, COLOR_PAIR(0));
    wprintw(status, "Press any key to continue");
    wnoutrefresh(status);

    doupdate();
}

static void draw_prompt(Session *sess)
{
    wmove(status, 0, 0);
    wbkgd(status, COLOR_PAIR(0));
    wattron(status, COLOR_PAIR(CP_STATUS));
    wprintw(status, sess->cmd_prompt.cmd_text); 
    wattroff(status, COLOR_PAIR(CP_STATUS));
    wprintw(status, " "); 

    size_t prompt_size = strlen(sess->cmd_prompt.cmd_text) + 1;
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
    WINDOW *draw_win = windows[buffer->win_info.draw_window];

    if (buffer->win_info.line_no_width > 0) {
        werase(lineno);
    }

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

    if (buffer->win_info.line_no_width > 0) {
        wnoutrefresh(lineno);
    }

    wstandend(draw_win);
    wattron(draw_win, COLOR_PAIR(CP_BUFFER_END));

    while (line_count < buffer->win_info.height) {
        mvwaddch(text, buffer->win_info.start_y + line_count++, 
                 buffer->win_info.start_x - buffer->win_info.line_no_width, '~');
    }

    wattroff(draw_win, COLOR_PAIR(CP_BUFFER_END));
}

static size_t draw_line(Buffer *buffer, BufferPos draw_pos, int y, int is_selection, 
                        Range select_range, int line_wrap, WindowInfo win_info)
{
    Line *line = draw_pos.line;

    if (win_info.line_no_width > 0 && draw_pos.offset == 0) {
        wmove(lineno, win_info.start_y + y, 0);
        wattron(lineno, COLOR_PAIR(CP_LINE_NO));
        wprintw(lineno, "%*zu ", ((int)win_info.line_no_width - 1), draw_pos.line_no);
        wattroff(lineno, COLOR_PAIR(CP_LINE_NO));
    }

    if (line->length == 0) {
        return 1;
    }

    CharInfo char_info;

    if (!line_wrap) {
        if (line->screen_length < draw_pos.col_no) {
            return 1;
        }

        size_t col_no = draw_pos.col_no;
        draw_pos.offset = 0;
        draw_pos.col_no = 1;

        while (draw_pos.col_no < col_no && draw_pos.offset < line->length) {
            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, draw_pos);

            /* TODO Also handle unprintable characters when horizontally scrolling */
            if (draw_pos.col_no + char_info.screen_length > col_no) {
                draw_pos.col_no = col_no;
                break;
            }

            draw_pos.offset += char_info.byte_length;
            draw_pos.col_no += char_info.screen_length;
        }

        if (draw_pos.col_no < col_no) {
            return 1;
        }
    }

    WINDOW *draw_win = windows[win_info.draw_window];
    size_t scr_line_num = 0;
    size_t start_col = draw_pos.col_no;
    size_t window_width = win_info.start_x + win_info.width;
    size_t window_height = win_info.start_y + win_info.height;
    size_t screen_length = 0;

    while (draw_pos.offset < line->length && scr_line_num < window_height) {
        wmove(draw_win, win_info.start_y + y + scr_line_num, win_info.start_x);

        for (screen_length += win_info.start_x; 
             screen_length < window_width && draw_pos.offset < line->length;) {

            if (is_selection && bufferpos_in_range(select_range, draw_pos)) {
                wattron(draw_win, A_REVERSE);
            } else {
                wattroff(draw_win, A_REVERSE);
            }

            buffer->cef.char_info(&char_info, CIP_SCREEN_LENGTH, draw_pos);

            draw_char(char_info, draw_pos, draw_win, window_width, line_wrap);

            draw_pos.col_no += char_info.screen_length;
            draw_pos.offset += char_info.byte_length;
            screen_length += char_info.screen_length;
        }

        scr_line_num++;

        if (!line_wrap) {
            break;
        }

        screen_length -= window_width;
    }

    if (scr_line_num < screen_height_from_screen_length(win_info, line->screen_length - (start_col - 1))) {
        scr_line_num++;
    }

    return scr_line_num;
}

static void draw_char(CharInfo char_info, BufferPos draw_pos, WINDOW *draw_win, size_t window_width, int line_wrap)
{
    uchar character = *(draw_pos.line->text + draw_pos.offset);
    size_t remaining = window_width - ((draw_pos.col_no - 1) % window_width);

    if (!char_info.is_valid) {
        waddstr(draw_win, "\xEF\xBF\xBD");
    } else if (!char_info.is_printable) {
        char nonprint_draw[] = "^ ";

        if (character == 127) {
            nonprint_draw[1] = '?';
        } else {
            nonprint_draw[1] = character + 64;
        }

        waddnstr(draw_win, nonprint_draw, line_wrap ? 2 : remaining);
    } else if (character == '\t') {
        for (size_t k = 0; k < char_info.screen_length && (line_wrap || k < remaining); k++) {
            waddstr(draw_win, " ");
        }
    } else {
        waddnstr(draw_win, draw_pos.line->text + draw_pos.offset, char_info.byte_length);
    }
}

static void position_cursor(Buffer *buffer, int line_wrap)
{
    WindowInfo win_info = buffer->win_info;
    WINDOW *draw_win = windows[win_info.draw_window];
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

static size_t update_line_no_width(Buffer *buffer, int line_wrap)
{
    BufferPos screen_start = buffer->screen_start;
    WindowInfo *win_info = &buffer->win_info;
    char lineno_str[50];

    size_t max_line_no;

    if (!config_bool("lineno")) {
        max_line_no = 0;
    } else if (line_wrap) {
        Line *line = screen_start.line;
        size_t screen_lines = screen_height_from_screen_length(*win_info, 
                line->screen_length - (screen_start.col_no - 1));

        while (line->next != NULL && screen_lines < win_info->height) {
            line = line->next;
            screen_lines += line_screen_height(*win_info, line);
            screen_start.line_no++;
        }

        max_line_no = screen_start.line_no;
    } else {
        max_line_no = screen_start.line_no + win_info->height - 1;
    }

    size_t line_no_width;

    if (max_line_no > 0) {
        line_no_width = snprintf(lineno_str, sizeof(lineno_str), "%zu ", max_line_no);
    } else {
        line_no_width = 0;
    }

    size_t diff = 0;

    if (line_no_width > win_info->line_no_width) {
        diff = line_no_width - win_info->line_no_width;
        win_info->width -= diff;
        win_info->line_no_width = line_no_width;
    } else if (line_no_width < win_info->line_no_width) {
        diff = win_info->line_no_width - line_no_width;
        win_info->width += diff;
        win_info->line_no_width = line_no_width;
    }

    if (diff > 0 || line_no_width != line_no_x) {
        if (line_wrap) {
            vertical_scroll_linewrap(buffer);
        }

        wresize(text, text_y, text_x - line_no_width);
        mvwin(text, 1, line_no_width);

        werase(lineno);
        wresize(lineno, text_y, line_no_width);

        line_no_x = line_no_width;
    }

    return line_no_width;
}
