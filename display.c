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
#include <assert.h>
#include "display.h"
#include "session.h"
#include "buffer.h"
#include "util.h"
#include "config.h"

#define WINDOW_NUM 4
#define STATUS_TEXT_SIZE 512
#define MAX_MENU_BUFFER_WIDTH 30
#define SC_COLOR_PAIR(screen_comp) (COLOR_PAIR((screen_comp) + 1))

/* TODO We don't want to use global static variables anywhere in wed */
/* Four windows are currently used for the wed display */
static WINDOW *menu_win; /* Displays open buffers */
static WINDOW *status_win; /* File status and prompt */
static WINDOW *text_win; /* Buffer content */
static WINDOW *line_no_win; /* Has zero width when ln=0 */
static WINDOW *windows[WINDOW_NUM]; /* Get window by DrawWindow index */
static size_t text_win_y = 0; /* Text window height */
static size_t text_win_x = 0; /* Text window width */

static void init_properties_and_windows(const Theme *);
static short ncurses_color(DrawColor);
static void draw_prompt(Session *);
static void draw_menu(Session *);
static void draw_status(Session *);
static size_t draw_status_file_info(Session *sess, size_t max_segment_width);
static size_t draw_status_pos_info(Session *sess, size_t max_segment_width);
static void draw_status_general_info(Session *sess, size_t file_info_size,
                                     size_t available_space);
static SyntaxMatches *setup_syntax(const Session *, Buffer *,
                                   const BufferPos *);
static void draw_buffer(const Session *, Buffer *, int line_wrap);
static size_t draw_line(Buffer *, BufferPos *, int y, int is_selection,
                        Range select_range, int line_wrap, WindowInfo win_info,
                        SyntaxMatches *syn_matches);
static void draw_char(CharInfo, const BufferPos *, WINDOW *,
                      size_t window_width, int line_wrap);
static void draw_line_ending(WINDOW *draw_win, const Range *select_range,
                             const BufferPos *draw_pos, int is_selection);
static void position_cursor(Buffer *, int line_wrap);
static void vertical_scroll(Buffer *);
static void vertical_scroll_linewrap(Buffer *);
static void horizontal_scroll(Buffer *);
static size_t update_line_no_width(Buffer *, int line_wrap);
static size_t line_screen_height(const Buffer *, const BufferPos *);

/* ncurses setup */
void init_display(const Theme *theme)
{
    initscr();

    text_win_y = LINES - 2;
    text_win_x = COLS;

    init_properties_and_windows(theme);
        
    refresh();
}

void init_display_test(void)
{
    text_win_x = 80;
    text_win_y = 24;
}

static void init_properties_and_windows(const Theme *theme)
{
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_color_pairs(theme);
    }

    /* See ncurses man pages for
     * explanations of these functions */
    raw();
    noecho();
    nl();
    keypad(stdscr, TRUE);
    curs_set(1);

    windows[0] = menu_win = newwin(1, COLS, 0, 0); 
    windows[1] = text_win = newwin(text_win_y, text_win_x, 1, 0);
    windows[2] = status_win = newwin(1, COLS, LINES - 1, 0);
    windows[3] = line_no_win = newwin(0, 0, 1, 0);
}

void resize_display(Session *sess)
{
    end_display();

    struct winsize win_size;
    /* Get terminal size */
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size) == -1) {
        /* TODO handle */
    }

    text_win_y = win_size.ws_row - 2;
    text_win_x = win_size.ws_col;

    resizeterm(win_size.ws_row, win_size.ws_col);

    init_properties_and_windows(se_get_active_theme(sess));
    init_all_window_info(sess);
    update_display(sess);
}

void suspend_display(void)
{
    endwin();
}

void end_display(void)
{
    delwin(menu_win);
    delwin(text_win);
    delwin(status_win);
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
    WindowInfo *prompt_win_info =
        &(pr_get_prompt_buffer(sess->prompt)->win_info);
    init_window_info(prompt_win_info);
    prompt_win_info->height = 1;
    prompt_win_info->draw_window = WIN_STATUS;
}

void init_window_info(WindowInfo *win_info)
{
    win_info->height = text_win_y;
    win_info->width = text_win_x;
    win_info->start_y = 0;
    win_info->start_x = 0;
    win_info->line_no_width = 0;
    win_info->horizontal_scroll = 1;
    win_info->draw_window = WIN_TEXT;
}

/* Map colors defined in a Theme to those ncurses uses */
static short ncurses_color(DrawColor draw_color)
{
    static short ncurses_colors[] = {
        [DC_NONE]    = -1,
        [DC_BLACK]   = COLOR_BLACK,
        [DC_RED]     = COLOR_RED,
        [DC_GREEN]   = COLOR_GREEN,
        [DC_YELLOW]  = COLOR_YELLOW,
        [DC_BLUE]    = COLOR_BLUE,
        [DC_MAGENTA] = COLOR_MAGENTA,
        [DC_CYAN]    = COLOR_CYAN,
        [DC_WHITE]   = COLOR_WHITE
    };

    assert(draw_color < ARRAY_SIZE(ncurses_colors, short));

    return ncurses_colors[draw_color];
}

/* Init color pairs based on theme. When the theme
 * is changed this function is called so that the
 * colors pairs can be updated */
void init_color_pairs(const Theme *theme)
{
    ThemeGroup group;

    for (size_t k = 0; k < SC_ENTRY_NUM; k++) {
        group = th_get_theme_group(theme, k);            
        init_pair(k + 1, ncurses_color(group.fg_color), 
                         ncurses_color(group.bg_color));
    }
}

/* Update the menu, status and active buffer views.
 * This is called after a change has been made that
 * needs to be reflected in the UI. */
void update_display(Session *sess)
{
    if (sess->wed_opt.test_mode) {
        return;
    }

    Buffer *buffer = sess->active_buffer;
    int line_wrap = cf_bool(buffer->config, CV_LINEWRAP);
    WINDOW *draw_win = windows[buffer->win_info.draw_window];

    if (line_wrap) {
        vertical_scroll_linewrap(buffer);
    } else {
        vertical_scroll(buffer);
        horizontal_scroll(buffer);
    }

    if (!se_prompt_active(sess)) {
        update_line_no_width(buffer, line_wrap);
    }

    draw_menu(sess);
    werase(draw_win);

    if (se_prompt_active(sess)) {
        draw_prompt(sess);
    } else {
        draw_status(sess);
    }

    draw_buffer(sess, buffer, line_wrap);

    position_cursor(buffer, line_wrap);

    doupdate();
}

/* TODO draw_menu and draw_status* need to consider file names with 
 * UTF-8 characters when calculating the screen space they will take.
 * i.e. use wsprintf instead of snprintf */
/* Draw top menu */
void draw_menu(Session *sess)
{
    /* Each tab has the format {Buffer Id} {Buffer Name} */
    const char *tab_fmt = " %zu %s ";
    char buffer_display[MAX_MENU_BUFFER_WIDTH];
    Buffer *buffer;
    size_t total_used_space = 0;
    size_t used_space = 0;

    /* Determine which buffer tab we will list first */
    if (sess->active_buffer_index < sess->menu_first_buffer_index) {
        sess->menu_first_buffer_index = sess->active_buffer_index;
    } else {
        buffer = sess->active_buffer;
        size_t start_index = sess->active_buffer_index;

        while (1) {
            used_space = snprintf(buffer_display, MAX_MENU_BUFFER_WIDTH,
                                  tab_fmt, start_index + 1,
                                  buffer->file_info.file_name);
            used_space = (used_space > MAX_MENU_BUFFER_WIDTH ?
                          MAX_MENU_BUFFER_WIDTH : used_space);

            if ((total_used_space + used_space > text_win_x) ||
                start_index == 0 || 
                start_index == sess->menu_first_buffer_index) {
                break;
            }

            total_used_space += used_space;
            buffer = se_get_buffer(sess, --start_index);
        }

        if (total_used_space + used_space > text_win_x) {
            sess->menu_first_buffer_index = start_index + 1;
        }  

        total_used_space = 0;
        used_space = 0;
    }

    buffer = se_get_buffer(sess, sess->menu_first_buffer_index);

    werase(menu_win);
    wbkgd(menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    wattron(menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));

    /* Draw as many tabs as we can fit on the screen */
    for (size_t buffer_index = sess->menu_first_buffer_index; 
         buffer_index < sess->buffer_num;
         buffer_index++) {

        used_space = snprintf(buffer_display, MAX_MENU_BUFFER_WIDTH, tab_fmt,
                              buffer_index + 1, buffer->file_info.file_name);
        used_space = (used_space > MAX_MENU_BUFFER_WIDTH ?
                      MAX_MENU_BUFFER_WIDTH : used_space);

        if (total_used_space + used_space > text_win_x) {
            break;
        }

        if (buffer_index == sess->active_buffer_index) {
            /* The active tab has custom coloring */
            wattron(menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
            mvwprintw(menu_win, 0, total_used_space, buffer_display); 
            wattroff(menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
        } else {
            mvwprintw(menu_win, 0, total_used_space, buffer_display); 
        }

        total_used_space += used_space;
        buffer = buffer->next;
    }

    wattroff(menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    /* Just update the virtual screen, as there are still further updates
     * to make */
    wnoutrefresh(menu_win); 
}

void draw_status(Session *sess)
{
    /* Split the status bar into 2 or 3 segments. Then determine
     * how much we can fit in each segment */
    int segment_num = 2;

    if (se_has_msgs(sess)) {
        segment_num = 3;
    }

    /* Segment 1: File info e.g. file path, file name, readonly, ...
     * Segment 2: Messages e.g. "Save Success" (Only exists if messages exist)
     * Segment 3: Position info e.g. Line No, Col No, ... */

    size_t max_segment_width = text_win_x / segment_num;

    werase(status_win);
    wmove(status_win, 0, 0);
    wbkgd(status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wattron(status_win, SC_COLOR_PAIR(SC_STATUS_BAR));

    size_t file_info_size = draw_status_file_info(sess, max_segment_width);
    size_t file_pos_size = draw_status_pos_info(sess, max_segment_width);

    if (segment_num == 3) {
        size_t available_space = text_win_x - file_info_size -
                                 file_pos_size - 1;
        draw_status_general_info(sess, file_info_size, available_space);
    }

    wattroff(status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wnoutrefresh(status_win); 
}

static size_t draw_status_file_info(Session *sess, size_t max_segment_width)
{
    char status_text[STATUS_TEXT_SIZE];
    size_t file_info_free = max_segment_width;
    const FileInfo *file_info = &sess->active_buffer->file_info;

    char *file_info_text = " ";

    if (!fi_file_exists(file_info)) {
        file_info_text = " [new] ";
    } else if (!fi_can_write_file(file_info)) {
        file_info_text = " [readonly] ";
    }

    file_info_free -= (strlen(file_info_text) + 3);
    size_t file_info_size;
    char *file_path = NULL;

    if (fi_file_exists(file_info)) {
        file_path = file_info->abs_path;
    } else if (fi_has_file_path(file_info)) {
        file_path = file_info->rel_path;
    }

    /* If we have the full file path and there's enough room 
     * to display it then do so, otherwise display the file name */
    if (file_path == NULL || strlen(file_path) > file_info_free) {
        file_path = file_info->file_name;
    }
    
    if (strlen(file_path) > file_info_free) {
        /* Print as much of the file name as we can
         * with 3 trailing dots to indicate it's not complete */
        int file_char_num = file_info_free - 3;
        file_info_size = snprintf(status_text, max_segment_width,
                                  " \"%.*s...\"%s", file_char_num,
                                  file_path, file_info_text);
    } else {
        file_info_size = snprintf(status_text, max_segment_width,
                                  " \"%s\"%s", file_path, file_info_text);
    }

    wprintw(status_win, status_text);

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

    size_t line_num = bf_lines(buffer);
    size_t lines_above = screen_start.line_no - 1;
    size_t lines_below;

    if ((screen_start.line_no + win_info.height - 1) >= line_num) {
        lines_below = 0; 
    } else {
        lines_below = line_num - (screen_start.line_no + win_info.height - 1);
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
        int pos_pct = (int)((lines_above /
                            (double)(lines_above + lines_below)) * 100);
        snprintf(rel_pos, sizeof(rel_pos), "%2d%%%%", pos_pct);
    }

    /* Attempt to print as much info as space allows */

    int pos_info_size = snprintf(status_text, STATUS_TEXT_SIZE,
                                 "Length: %zu Lines: %zu | "
                                 "Offset: %zu Line: %zu Col: %zu | "
                                 "%s ",
                                 bf_length(buffer), line_num,
                                 pos.offset, pos.line_no, pos.col_no,
                                 rel_pos);

    if (pos_info_size < 0 || (size_t)pos_info_size > max_segment_width) {
        pos_info_size = snprintf(status_text, max_segment_width,
                                 "Line: %zu Col: %zu ",
                                 pos.line_no, pos.col_no);
    }

    if (pos_info_size < 0 || (size_t)pos_info_size > max_segment_width) {
        pos_info_size = snprintf(status_text, max_segment_width, 
                                 "L:%zu C:%zu ", pos.line_no, pos.col_no);
    }

    mvwprintw(status_win, 0, text_win_x - strlen(status_text) - 1, status_text);

    return pos_info_size;
}

static void draw_status_general_info(Session *sess, size_t file_info_size,
                                     size_t available_space)
{
    char status_text[STATUS_TEXT_SIZE];
    char *msg = bf_join_lines(sess->msg_buffer, ". ");
    se_clear_msgs(sess);

    if (msg == NULL) {
        return;
    }

    available_space -= 3;
    mvwprintw(status_win, 0, file_info_size - 1, " | ");

    size_t msg_length = strlen(msg);

    if (msg_length > available_space) {
        /* TODO F12 functionality to view full message text not implemented */
        char *fmt = "%%.%ds... (F12 view full) |";
        char msg_fmt[STATUS_TEXT_SIZE];
        msg_length = available_space - strlen(fmt) + 5;
        snprintf(msg_fmt, STATUS_TEXT_SIZE, fmt, msg_length);
        snprintf(status_text, available_space, msg_fmt, msg);
    } else {
        snprintf(status_text, available_space, "%s", msg); 
    }

    mvwprintw(status_win, 0, file_info_size - 1 + 3, status_text);
    free(msg);
}

void draw_errors(Session *sess)
{
    Buffer *error_buffer = sess->error_buffer;
    BufferPos pos;
    size_t screen_lines = 0;
    WindowInfo *win_info = &error_buffer->win_info;
    WindowInfo win_info_orig = *win_info;
    bp_init(&pos, error_buffer->data,
            &error_buffer->file_format, error_buffer->config);

    while (!bp_at_buffer_end(&pos)) {
        screen_lines += line_screen_height(error_buffer, &pos);
        bp_to_line_end(&pos);
        bp_next_char(&pos);
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

    wattron(text_win, SC_COLOR_PAIR(SC_ERROR_MESSAGE));
    draw_buffer(sess, error_buffer, 1);
    wattroff(text_win, SC_COLOR_PAIR(SC_ERROR_MESSAGE));
    wclrtoeol(text_win);
    wnoutrefresh(text_win);

    *win_info = win_info_orig;

    wmove(status_win, 0, 0);
    werase(status_win);
    /* Clear any previous background color in status window */
    wbkgd(status_win, COLOR_PAIR(0));
    wprintw(status_win, "Press any key to continue");
    wnoutrefresh(status_win);

    doupdate();
}

static void draw_prompt(Session *sess)
{
    Prompt *prompt = sess->prompt;
    Buffer *prompt_buffer = pr_get_prompt_buffer(prompt);
    const char *prompt_text = pr_get_prompt_text(prompt);

    wmove(status_win, 0, 0);
    wbkgd(status_win, COLOR_PAIR(0));
    wattron(status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wprintw(status_win, prompt_text); 
    wattroff(status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wprintw(status_win, " "); 

    size_t prompt_size = strlen(prompt_text) + 1;
    WindowInfo *win_info = &prompt_buffer->win_info;
    win_info->start_x = prompt_size;
    win_info->width = text_win_x - prompt_size;

    pr_hide_suggestion_prompt(prompt);
}

static SyntaxMatches *setup_syntax(const Session *sess, Buffer *buffer, 
                                   const BufferPos *draw_pos)
{
    const SyntaxDefinition *syn_def = se_get_syntax_def(sess, buffer);

    if (syn_def == NULL) {
        return NULL;
    }

    /* Look ahead and behind from the current visible part of the buffer
     * by 20 lines when determining syntax matches. This is to try and ensure
     * constructs that span many lines, such as comments, which can start or
     * end outside of the visible buffer area are matched and highlighted.
     * Of course this isn't always enough for large comments and adds
     * a load overhead */
    BufferPos syn_start = *draw_pos;
    bf_change_multi_line(buffer, &syn_start, DIRECTION_UP, 20, 0);

    BufferPos syn_end = *draw_pos;
    bf_change_multi_line(buffer, &syn_end, DIRECTION_DOWN,
                         buffer->win_info.height + 20, 0);

    size_t syn_examine_length = syn_end.offset - syn_start.offset;
    char *syn_examine_text = malloc(syn_examine_length);

    if (syn_examine_text == NULL) {
        return NULL;
    }

    syn_examine_length = gb_get_range(buffer->data, syn_start.offset, 
                                      syn_examine_text, syn_examine_length);

    SyntaxMatches *syn_matches = sy_get_syntax_matches(syn_def,
                                                       syn_examine_text,
                                                       syn_examine_length,
                                                       syn_start.offset);

    free(syn_examine_text);
    
    return syn_matches;
}

static void draw_buffer(const Session *sess, Buffer *buffer, int line_wrap)
{
    Range select_range;
    int is_selection = bf_get_range(buffer, &select_range);
    size_t line_num = buffer->win_info.height;
    size_t line_count = 0;
    BufferPos draw_pos = buffer->screen_start;
    WINDOW *draw_win = windows[buffer->win_info.draw_window];
    size_t buffer_len = bf_length(buffer);
    SyntaxMatches *syn_matches = setup_syntax(sess, buffer, &draw_pos);

    if (!line_wrap) {
        size_t buffer_lines = bf_lines(buffer);

        if (line_num > buffer_lines) {
            line_num = buffer_lines;
        }
    }

    if (buffer->win_info.line_no_width > 0) {
        werase(line_no_win);
    }

    while (line_count < line_num && draw_pos.offset <= buffer_len) {
        line_count += draw_line(buffer, &draw_pos, line_count, is_selection, 
                                select_range, line_wrap, buffer->win_info,
                                syn_matches);

        if (draw_pos.offset == buffer_len) {
            break;
        }

        bp_next_line(&draw_pos);
    }

    if (buffer->win_info.line_no_width > 0) {
        wnoutrefresh(line_no_win);
    }

    wstandend(draw_win);
    wattron(draw_win, SC_COLOR_PAIR(SC_BUFFER_END));

    while (line_count < buffer->win_info.height) {
        mvwaddch(text_win, buffer->win_info.start_y + line_count++, 
                 buffer->win_info.start_x, '~');
    }

    wattroff(draw_win, SC_COLOR_PAIR(SC_BUFFER_END));
    free(syn_matches);
}

static size_t draw_line(Buffer *buffer, BufferPos *draw_pos, int y,
                        int is_selection, Range select_range, int line_wrap,
                        WindowInfo win_info, SyntaxMatches *syn_matches)
{
    /* Draw line numbers */
    if (win_info.line_no_width > 0 &&
        (bp_at_line_start(draw_pos) || !line_wrap)) {
        wmove(line_no_win, win_info.start_y + y, 0);
        wattron(line_no_win, SC_COLOR_PAIR(SC_LINENO));
        wprintw(line_no_win, "%*zu ", ((int)win_info.line_no_width - 1),
                draw_pos->line_no);
        wattroff(line_no_win, SC_COLOR_PAIR(SC_LINENO));
    }

    WINDOW *draw_win = windows[win_info.draw_window];
    const size_t window_width = win_info.start_x + win_info.width;
    const size_t window_height = win_info.start_y + win_info.height;

    /* Empty line */
    if (bp_at_line_end(draw_pos)) {
        if (line_wrap || win_info.horizontal_scroll == 1) {
            wmove(draw_win, win_info.start_y + y, win_info.start_x);
            draw_line_ending(draw_win, &select_range, draw_pos, is_selection);
        }

        return 1;
    }

    CharInfo char_info;

    if (!line_wrap && win_info.horizontal_scroll > 1) {
        /* draw_pos is at the start of the line and needs to be advanced
         * to column win_info.horizontal_scroll so that we can start drawing
         * this line */
        while (draw_pos->col_no < win_info.horizontal_scroll &&
               !bp_at_line_end(draw_pos)) {

            en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, draw_pos,
                              buffer->config);

            /* TODO Also handle unprintable characters when
             * horizontally scrolling */
            if (draw_pos->col_no + char_info.screen_length >
                    win_info.horizontal_scroll) {
                draw_pos->col_no = win_info.horizontal_scroll;
                break;
            }

            draw_pos->offset += char_info.byte_length;
            draw_pos->col_no += char_info.screen_length;
        }

        if (draw_pos->col_no < win_info.horizontal_scroll ||
            bp_at_line_end(draw_pos)) {
            /* This line is too short to be displayed with this
             * much horizontal scrolling */

            /* If the line ending is inside a selection however then it may
             * be far along enough horizontally to appear on the screen */
            if (draw_pos->col_no == win_info.horizontal_scroll &&
                bp_at_line_end(draw_pos)) {
                wmove(draw_win, win_info.start_y + y, win_info.start_x);
                draw_line_ending(draw_win, &select_range,
                                 draw_pos, is_selection);
            }

            return 1;
        }
    }

    size_t scr_line_num = 0;
    size_t start_col = draw_pos->col_no;
    size_t screen_length = 0;
    const SyntaxMatch *syn_match;

    while (!bp_at_line_end(draw_pos) && scr_line_num < window_height) {
        /* When linewrap is enabled a single line can be displayed as 
         * multiple screen lines due to wrapping */
        wmove(draw_win, win_info.start_y + y + scr_line_num, 
                        /* screen_length here handles any wrapping
                         * from the previous line when drawing a character
                         * that takes up multiple columns at the end of
                         * the previous line */
                        win_info.start_x + screen_length);

        for (screen_length += win_info.start_x; 
             screen_length < window_width && !bp_at_line_end(draw_pos);) {

            if (is_selection && bf_bp_in_range(&select_range, draw_pos)) {
                /* Highlight selected text */
                wattron(draw_win, A_REVERSE);
            } else {
                wattroff(draw_win, A_REVERSE);
            }

            if (syn_matches != NULL && syn_matches->match_num > 0) {
                syn_match = sy_get_syntax_match(syn_matches, draw_pos->offset);

                /* If we're in a token then apply syntax highlighting */
                if (syn_match == NULL) {
                    wattron(draw_win, SC_COLOR_PAIR(ST_NORMAL));
                } else {
                    wattron(draw_win, SC_COLOR_PAIR(syn_match->token));
                }
            }

            en_utf8_char_info(&char_info, CIP_SCREEN_LENGTH, draw_pos,
                              buffer->config);

            draw_char(char_info, draw_pos, draw_win, window_width, line_wrap);

            draw_pos->col_no += char_info.screen_length;
            draw_pos->offset += char_info.byte_length;
            screen_length += char_info.screen_length;
        }

        scr_line_num++;

        if (!line_wrap) {
            /* There's nothing more to draw as we don't wrap 
             * the remainder of the line text */
            break;
        }

        screen_length -= window_width;
    }

    if (line_wrap || (draw_pos->col_no - start_col) < window_width) {
        draw_line_ending(draw_win, &select_range, draw_pos, is_selection);
    }

    size_t col_diff = draw_pos->col_no - start_col;

    /* When a line ends at the last column of the screen then we need to
     * add a line below to allow the cursor to be placed at the end of the
     * line i.e. in a position where the line can be appended to.
     * We can check this simply by checking if the screen height we measured
     * drawing the line matches the amount that is expected */
    if (scr_line_num < screen_height_from_screen_length(buffer, col_diff)) {
        scr_line_num++;
    }

    return scr_line_num;
}

static void draw_char(CharInfo char_info, const BufferPos *draw_pos,
                      WINDOW *draw_win, size_t window_width, int line_wrap)
{
    uchar character[50] = { '\0' };
    gb_get_range(draw_pos->data, draw_pos->offset, (char *)character,
                 char_info.byte_length);
    size_t remaining = window_width - ((draw_pos->col_no - 1) % window_width);

    if (!char_info.is_valid) {
        /* Unicode replacement character */
        waddstr(draw_win, "\xEF\xBF\xBD");
    } else if (!char_info.is_printable) {
        char nonprint_draw[] = "^ ";

        if (*character == 127) {
            nonprint_draw[1] = '?';
        } else {
            nonprint_draw[1] = character[0] + 64;
        }

        /* If linewrap is enabled then ncurses will wrap the second 
         * character onto the next line down for us if necessary */
        waddnstr(draw_win, nonprint_draw, line_wrap ? 2 : remaining);
    } else if (*character == '\t') {
        for (size_t k = 0;
            k < char_info.screen_length && (line_wrap || k < remaining);
            k++) {
            waddstr(draw_win, " ");
        }
    } else {
        waddnstr(draw_win, (char *)character, char_info.byte_length);
    }
}

static void draw_line_ending(WINDOW *draw_win, const Range *select_range,
                             const BufferPos *draw_pos, int is_selection)
{
    if (is_selection && bp_at_line_end(draw_pos) &&
        bf_bp_in_range(select_range, draw_pos)) {
        wattron(draw_win, A_REVERSE);
        waddstr(draw_win, " ");
    }
}

/* After drawing buffer place cursor in correct position on screen */
static void position_cursor(Buffer *buffer, int line_wrap)
{
    WindowInfo win_info = buffer->win_info;
    WINDOW *draw_win = windows[win_info.draw_window];
    BufferPos pos = buffer->pos;
    BufferPos screen_start = buffer->screen_start;
    size_t cursor_y = 0, cursor_x;

    if (line_wrap) {
        while (screen_start.line_no < pos.line_no) {
            cursor_y += line_screen_height(buffer, &screen_start);
            bp_next_line(&screen_start);
        }

        size_t screen_length = pos.col_no - screen_start.col_no;
        cursor_y += win_info.start_y +
                    screen_height_from_screen_length(buffer, screen_length) - 1;
        cursor_x = win_info.start_x + (screen_length %= win_info.width);
    } else {
        cursor_y = win_info.start_y + pos.line_no - screen_start.line_no;
        cursor_x = win_info.start_x + pos.col_no - win_info.horizontal_scroll;
    }

    wmove(draw_win, cursor_y, cursor_x);
    wnoutrefresh(draw_win);
}

/* Convert column number into screen column number */
size_t screen_col_no(const Buffer *buffer, const BufferPos *pos)
{
    size_t col_no;

    if (cf_bool(buffer->config, CV_LINEWRAP)) {
        col_no = ((pos->col_no - 1) % buffer->win_info.width) + 1;
    } else {
        col_no = pos->col_no;
    }

    return col_no;
}

/* Returns the number of screen lines the content from the provided pos to 
 * the end of the line will take. This function is only needed when
 * linewrap is enabled */ 
static size_t line_screen_height(const Buffer *buffer, const BufferPos *pos)
{
    BufferPos tmp = *pos;
    const WindowInfo *win_info = &buffer->win_info;
    /* Instead of advancing the pos to the end of the line, simply advance it
     * as far as we could possibly view on the screen. This is to help deal
     * with very long lines i.e. think a single line 100MB+ in length */
    size_t max_visible_col = pos->col_no + (win_info->height * win_info->width);
    bp_advance_to_col(&tmp, max_visible_col);

    size_t screen_length = tmp.col_no - pos->col_no;

    return screen_height_from_screen_length(buffer, screen_length);
}

/* This calculates the number of lines that text, when displayed on the screen
 * takes up i.e. the number of screen lines */
size_t screen_height_from_screen_length(const Buffer *buffer,
                                        size_t screen_length)
{
    if (!cf_bool(buffer->config, CV_LINEWRAP) || screen_length == 0) {
        /* Without wrapping a line can only ever take one screen line */
        return 1;
    } else if ((screen_length % buffer->win_info.width) == 0) {
        /* Allow space for cursor at the end of line */
        screen_length++;
    }

    return roundup_div(screen_length, buffer->win_info.width);
}

/* TODO consider using ncurses scroll function as well */
/* Determine if the screen needs to be scrolled and 
 * how much if so. There is a separate scroll function
 * when linewrap is enabled */
static void vertical_scroll(Buffer *buffer)
{
    WindowInfo win_info = buffer->win_info;
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &buffer->screen_start;
    bp_to_line_start(screen_start);

    if (pos.line_no < screen_start->line_no) {
        /* If pos is now before the start of the buffer content we're
         * currently displaying, then start displaying the buffer from
         * the same line pos is on */
        BufferPos tmp = pos;
        bp_to_line_start(&tmp);
        screen_start->offset = tmp.offset;
        screen_start->line_no = tmp.line_no;
    } else {
        size_t diff = pos.line_no - screen_start->line_no;

        /* pos still appears on screen with current screen_start */
        if (diff < win_info.height) {
            return;
        }

        diff -= (win_info.height - 1);

        if (diff > win_info.height) {
            /* pos is beyond the end of the current buffer content displayed,
             * so start displaying from same line as pos */
            BufferPos tmp = pos;
            bp_to_line_start(&tmp);
            screen_start->offset = tmp.offset;
            screen_start->line_no = tmp.line_no;
        } else {
            /* pos is only a couple of lines below the end of the buffer
             * content displayed, so scroll screen_start down until pos
             * comes into view. This allows us to scroll down through
             * the buffer smoothly from the users perspective */
            bf_change_multi_line(buffer, screen_start, DIRECTION_DOWN, diff, 0);
        }
    }
}

static void vertical_scroll_linewrap(Buffer *buffer)
{
    BufferPos pos = buffer->pos;
    BufferPos *screen_start = &buffer->screen_start;

    if ((pos.line_no < screen_start->line_no) ||
        (pos.line_no == screen_start->line_no &&
         pos.col_no < screen_start->col_no)) {
        /* Start displaying from the same line pos is on, as it's
         * before our current screen_start */
        *screen_start = pos;

        if (!bf_bp_at_screen_line_start(buffer, screen_start)) {
            bf_bp_to_screen_line_start(buffer, screen_start, 0, 0);
        }
    } else {
        /* Reverse back from pos until we encounter screen_start or traverse
         * the height of the screen. If we don't encounter screen_start
         * then start from where we traversed back to */
        BufferPos start = pos;

        if (!bf_bp_at_screen_line_start(buffer, &start)) {
            bf_bp_to_screen_line_start(buffer, &start, 0, 0);
        }

        size_t line_num = buffer->win_info.height;

        while (bp_compare(&start, screen_start) != 0 && --line_num > 0) {
            bf_change_line(buffer, &start, DIRECTION_UP, 0);
        }

        if (line_num == 0) {
            *screen_start = start;
        }
    }
}

/* Only called when linewrap=false */
static void horizontal_scroll(Buffer *buffer)
{
    size_t diff;
    Direction direction;
    BufferPos pos = buffer->pos;
    WindowInfo *win_info = &buffer->win_info;

    /* win_info->horizontal_scroll is the column we're currently
     * starting to display each line from */

    if (pos.col_no > win_info->horizontal_scroll) {
        diff = pos.col_no - win_info->horizontal_scroll;
        direction = DIRECTION_RIGHT;
    } else {
        diff = win_info->horizontal_scroll - pos.col_no;
        direction = DIRECTION_LEFT;
    }

    if (diff == 0) {
        return;
    }

    if (direction == DIRECTION_RIGHT) {
        if (diff < win_info->width) {
            return;
        }

        diff -= (win_info->width - 1);

        win_info->horizontal_scroll += diff;
    } else {
        win_info->horizontal_scroll -= diff;
    }
}

/* Calculate the screen width required to display line numbers */
static size_t update_line_no_width(Buffer *buffer, int line_wrap)
{
    static size_t line_no_x = 0; /* The current width of ncurses line no 
                                    window */
    BufferPos screen_start = buffer->screen_start;
    WindowInfo *win_info = &buffer->win_info;
    char lineno_str[50];

    size_t max_line_no;

    if (!cf_bool(buffer->config, CV_LINENO)) {
        max_line_no = 0;
    /* Work out the maximum line number that could be displayed. */
    } else if (line_wrap) {
        size_t screen_lines = 0;

        while (!bp_at_buffer_end(&screen_start) &&
                screen_lines < win_info->height) {
            screen_lines += line_screen_height(buffer, &screen_start);
            bp_next_line(&screen_start);
        }

        max_line_no = screen_start.line_no;
    } else {
        max_line_no = screen_start.line_no + win_info->height - 1;
    }

    size_t line_no_width;

    if (max_line_no > 0) {
        line_no_width = snprintf(lineno_str, sizeof(lineno_str), "%zu ",
                                 max_line_no);
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
        /* We need to resize the line_no ncurses window and the text window
         * to make room for it */
        if (line_wrap) {
            vertical_scroll_linewrap(buffer);
        }

        wresize(text_win, text_win_y, text_win_x - line_no_width);
        mvwin(text_win, 1, line_no_width);

        werase(line_no_win);
        wresize(line_no_win, text_win_y, line_no_width);

        line_no_x = line_no_width;
    }

    return line_no_width;
}
