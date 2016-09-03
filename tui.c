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

#include <string.h>
#include <sys/ioctl.h>
#include "tui.h"
#include "util.h"

#define SC_COLOR_PAIR(screen_comp) (COLOR_PAIR((screen_comp) + 1))

static Status ti_init(UI *);
static Status ti_get_input(UI *);
static void ti_init_display(TUI *);
static short ti_get_ncurses_color(DrawColor);
static Status ti_update(UI *);
static void ti_draw_buffer_tabs(TUI *);
static void ti_draw_line_no(TUI *);
static void ti_draw_buffer(TUI *);
static void ti_draw_buffer_view(const BufferView *, WINDOW *);
static int ti_draw_buffer_line(WINDOW *, const BufferView *, const Line *);
static int ti_draw_buffer_cell(WINDOW *, const Cell *);
static void ti_draw_status_bar(TUI *);
static void ti_draw_prompt(TUI *);
static void ti_position_cursor(TUI *);
static Status ti_error(UI *);
static Status ti_update_theme(UI *);
static Status ti_resize(UI *);
static Status ti_suspend(UI *);
static Status ti_end(UI *);
static Status ti_free(UI *);

UI *ti_new(Session *sess)
{
    TUI *tui = calloc(1, sizeof(TUI));
    RETURN_IF_NULL(tui);    

    tui->sess = sess;

    tui->ui.init = ti_init;
    tui->ui.get_input = ti_get_input;
    tui->ui.update = ti_update;
    tui->ui.error = ti_error;
    tui->ui.update_theme = ti_update_theme;
    tui->ui.resize = ti_resize;
    tui->ui.suspend = ti_suspend;
    tui->ui.end = ti_end;
    tui->ui.free = ti_free;

    return (UI *)tui;
}

static Status ti_init(UI *ui)
{
    TUI *tui = (TUI *)ui;

    /* Termkey */
    /* Create new termkey instance monitoring stdin with
     * the SIGINT behaviour of Ctrl-C disabled */
    tui->termkey = termkey_new(STDIN_FILENO,
                               TERMKEY_FLAG_SPACESYMBOL | TERMKEY_FLAG_CTRLC);

    if (tui->termkey == NULL) {
        return st_get_error(ERR_UNABLE_TO_INITIALISE_TERMEKEY,
                            "Unable to create termkey instance");
    }

    /* Represent ASCII DEL character as backspace */
    termkey_set_canonflags(tui->termkey,
                           TERMKEY_CANON_DELBS | TERMKEY_CANON_SPACESYMBOL);

    if (tui->sess->wed_opt.test_mode) {
        tui->rows = 24;
        tui->cols = 80;
        return STATUS_SUCCESS;
    }

    /* ncurses */
    initscr();
    tui->rows = LINES;
    tui->cols = COLS;
    tv_init(&tui->tv, tui->rows, tui->cols);
    ti_init_display(tui);
    refresh();

    return STATUS_SUCCESS;
}

static void ti_init_display(TUI *tui)
{
    if (has_colors()) {
        start_color();
        use_default_colors();
        ti_update_theme((UI *)tui);
    }

    /* See ncurses man pages for
     * explanations of these functions */
    raw();
    noecho();
    nl();
    keypad(stdscr, TRUE);
    curs_set(1);

    tui->menu_win = newwin(1, tui->cols, 0, 0); 
    tui->buffer_win = newwin(tui->rows - 2, tui->cols, 1, 0);
    tui->status_win = newwin(1, tui->cols, tui->rows - 1, 0);
    tui->line_no_win = newwin(0, 0, 1, 0);
}

static short ti_get_ncurses_color(DrawColor draw_color)
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

static Status ti_get_input(UI *ui)
{
    TUI *tui = (TUI *)ui;
    TermKey *termkey = tui->termkey;
    InputBuffer *input_buffer = &tui->sess->input_buffer;
    char keystr[MAX_KEY_STR_SIZE];
    TermKeyResult ret;
    TermKeyKey key;
    size_t keystr_len;
    size_t keys_added = 0;

    if (input_buffer->arg == IA_INPUT_AVAILABLE_TO_READ) {
        /* Inform termkey input is available to be read */
        termkey_advisereadable(termkey);

        while ((ret = termkey_getkey(termkey, &key)) == TERMKEY_RES_KEY) {
            keystr_len = termkey_strfkey(termkey, keystr, MAX_KEY_STR_SIZE,
                                         &key, TERMKEY_FORMAT_VIM);

            RETURN_IF_FAIL(ip_add_keystr_input_to_end(input_buffer, keystr,
                                                      keystr_len));

            keys_added++;
        }

        if (ret == TERMKEY_RES_AGAIN) {
            /* Partial keypress found, try waiting for more input */
            input_buffer->wait_time_nano = termkey_get_waittime(termkey) * 1000;
            input_buffer->result = IR_WAIT_FOR_MORE_INPUT;
        } else if (ret == TERMKEY_RES_EOF) {
            input_buffer->result = IR_EOF;
        } else if (keys_added > 0) {
            input_buffer->result = IR_INPUT_ADDED;
        } else {
            input_buffer->result = IR_NO_INPUT_ADDED;
        }
    } else if (input_buffer->arg == IA_NO_INPUT_AVAILABLE_TO_READ) {
        /* Attempt to interpret any unprocessed input as a key */
        if (termkey_getkey_force(termkey, &key) == TERMKEY_RES_KEY) {
            keystr_len = termkey_strfkey(termkey, keystr, MAX_KEY_STR_SIZE,
                                         &key, TERMKEY_FORMAT_VIM);

            RETURN_IF_FAIL(ip_add_keystr_input_to_end(input_buffer, keystr,
                                                      keystr_len));
                
            input_buffer->result = IR_INPUT_ADDED;
        } else {
            input_buffer->result = IR_NO_INPUT_ADDED;
        }
    } else {
        assert(!"Unhandled InputArgument");
    }

    return STATUS_SUCCESS;
}

static Status ti_update(UI *ui)
{
    TUI *tui = (TUI *)ui;

    if (tui->sess->wed_opt.test_mode) {
        return STATUS_SUCCESS;
    }

    RETURN_IF_FAIL(tv_update(&tui->tv, tui->sess));
    ti_draw_buffer_tabs(tui);

    if (tui->tv.is_prompt_active) {
        ti_draw_prompt(tui);
    } else {
        ti_draw_line_no(tui);
        ti_draw_buffer(tui);
        ti_draw_status_bar(tui);
    }

    ti_position_cursor(tui);

    doupdate();

    return STATUS_SUCCESS;
}

static void ti_draw_buffer_tabs(TUI *tui)
{
    const TabbedView *tv = &tui->tv;
    assert(tui->sess->active_buffer_index >= tui->tv.first_buffer_tab_index);
    size_t active_buffer_index = tui->sess->active_buffer_index -
                                 tui->tv.first_buffer_tab_index;
    assert(active_buffer_index < tv->buffer_tab_num);

    wmove(tui->menu_win, 0, 0);
    wbkgd(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    wattron(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));

    for (size_t k = 0; k < tv->buffer_tab_num; k++) {
        if (k == active_buffer_index) {
            /* The active tab has custom coloring */
            wattron(tui->menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
            waddstr(tui->menu_win, tv->buffer_tabs[k]); 
            wattroff(tui->menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
        } else {
            waddstr(tui->menu_win, tv->buffer_tabs[k]); 
        } 
    }

    wclrtoeol(tui->menu_win);
    wattroff(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    wnoutrefresh(tui->menu_win); 
}

static void ti_draw_line_no(TUI *tui)
{
    int win_x, win_y;
    const BufferView *bv = tui->tv.bv;
    (void)win_y;

    getmaxyx(tui->line_no_win, win_y, win_x);

    if (tui->tv.line_no_width == 0 && tui->tv.line_no_width == (size_t)win_x) {
        return;
    } else if (tui->tv.line_no_width != tui->tv.last_line_no_width) {
        wresize(tui->buffer_win, bv->rows, bv->cols);
        mvwin(tui->buffer_win, 1, tui->tv.line_no_width);
        werase(tui->line_no_win);
        wresize(tui->line_no_win, bv->rows, tui->tv.line_no_width);
    }

    const Line *line;

    for (size_t row = 0; row < bv->rows; row++) {
        line = &bv->lines[row];
        wmove(tui->line_no_win, row, 0);

        if (line->line_no != 0) {
            wattron(tui->line_no_win, SC_COLOR_PAIR(SC_LINENO));
            wprintw(tui->line_no_win, "%*zu ",
                    ((int)tui->tv.line_no_width - 1), line->line_no);
            wattroff(tui->line_no_win, SC_COLOR_PAIR(SC_LINENO));
        } else {
            wprintw(tui->line_no_win, "%*s ",
                    ((int)tui->tv.line_no_width - 1), "");
        }
    }

    wnoutrefresh(tui->line_no_win); 
}

static void ti_draw_buffer(TUI *tui)
{
    wmove(tui->buffer_win, 0, 0);
    ti_draw_buffer_view(tui->tv.bv, tui->buffer_win);
}

static void ti_draw_buffer_view(const BufferView *bv, WINDOW *win)
{
    const Line *line;

    for (size_t row = 0; row < bv->rows; row++) {
        line = &bv->lines[row];

        if (!ti_draw_buffer_line(win, bv, line)) {
            break;
        }
    }

    wclrtobot(win);
    wnoutrefresh(win); 
}

static int ti_draw_buffer_line(WINDOW *win, const BufferView *bv,
                               const Line *line)
{
    const Cell *cell;

    for (size_t col = 0; col < bv->cols; col++) {
        cell = &line->cells[col]; 

        if (cell->text_len == 0) {
            continue;
        }

        if (!ti_draw_buffer_cell(win, cell)) {
            return 0;
        }
    }

    return 1;
}

static int ti_draw_buffer_cell(WINDOW *win, const Cell *cell)
{
    attr_t attr = A_NORMAL;

    if (cell->attr & CA_SELECTION && !(cell->attr & CA_SEARCH_MATCH)) {
        attr |= A_REVERSE;
    }

    if (cell->attr & CA_ERROR) {
        attr |= SC_COLOR_PAIR(SC_ERROR_MESSAGE);
    } else if (cell->attr & CA_COLORCOLUMN) {
        attr |= SC_COLOR_PAIR(SC_COLORCOLUMN);
    } else if (cell->attr & CA_SEARCH_MATCH) {
        if (cell->attr & CA_SELECTION) {
            attr |= SC_COLOR_PAIR(SC_PRIMARY_SEARCH_MATCH);
        } else {
            attr |= SC_COLOR_PAIR(SC_SEARCH_MATCH);
        }
    } else if ((cell->attr & CA_BUFFER_END) || (cell->attr & CA_WRAP)) {
        attr |= SC_COLOR_PAIR(SC_BUFFER_END);
    } else {
        attr |= SC_COLOR_PAIR(cell->token);
    }

    wattrset(win, attr);
    waddnstr(win, cell->text, cell->text_len);

    if (cell->attr & CA_BUFFER_END) {
        wclrtoeol(win);
        int x, y;
        (void)x;
        getyx(win, y, x); 
        wmove(win, ++y, 0);
    }

    return 1;
}

static void ti_draw_status_bar(TUI *tui)
{
    const TabbedView *tv = &tui->tv;

    werase(tui->status_win);
    wmove(tui->status_win, 0, 0);
    wbkgd(tui->status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wattron(tui->status_win, SC_COLOR_PAIR(SC_STATUS_BAR));

    wprintw(tui->status_win, tv->status_bar[0]);

    if (strlen(tv->status_bar[1])) {
        wprintw(tui->status_win, "| ");
        wprintw(tui->status_win, tv->status_bar[1]);
    }

    size_t pos_info_len = strlen(tv->status_bar[2]);
    mvwprintw(tui->status_win, 0, tui->cols - pos_info_len - 1,
              tv->status_bar[2]);

    wattroff(tui->status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wnoutrefresh(tui->status_win); 
}

static void ti_draw_prompt(TUI *tui)
{
    const Session *sess = tui->sess;
    Prompt *prompt = sess->prompt;
    const char *prompt_text = tui->tv.prompt_text;

    wmove(tui->status_win, 0, 0);
    wbkgd(tui->status_win, COLOR_PAIR(0));
    wattron(tui->status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wprintw(tui->status_win, prompt_text); 
    wattroff(tui->status_win, SC_COLOR_PAIR(SC_STATUS_BAR));
    wprintw(tui->status_win, " "); 

    pr_hide_suggestion_prompt(prompt);

    const BufferView *bv = sess->active_buffer->bv;
    ti_draw_buffer_view(bv, tui->status_win);
}

static void ti_position_cursor(TUI *tui)
{
    const BufferView *bv = tui->tv.bv;
    const Line *line;
    const Cell *cell;
    size_t screen_col;

    for (size_t row = 0; row < bv->rows; row++) {
        line = &bv->lines[row];
        screen_col = 0;

        for (size_t col = 0; col < bv->cols; col++) {
            cell = &line->cells[col]; 

            if (cell->attr & CA_CURSOR) {
                WINDOW *win;

                if (tui->tv.is_prompt_active) {
                    win = tui->status_win;
                    screen_col += tui->tv.prompt_text_len + 1;
                } else {
                    win = tui->buffer_win;
                }

                wmove(win, row, screen_col);
                wnoutrefresh(win);
                return;
            } else if (cell->text_len > 0) {
                screen_col += cell->col_width;
            }
        }
    }
}

static Status ti_error(UI *ui)
{
    TUI *tui = (TUI *)ui;
    Session *sess = tui->sess;

    if (tui->sess->wed_opt.test_mode) {
        return STATUS_SUCCESS;
    }

    sess->error_buffer->next = sess->active_buffer;
    sess->active_buffer = sess->error_buffer;
    Status status = tv_update(&tui->tv, tui->sess);
    sess->active_buffer = sess->error_buffer->next;
    RETURN_IF_FAIL(status);

    BufferView *bv = tui->tv.bv;
    bv_apply_cell_attributes(bv, CA_ERROR, CA_LINE_END | CA_NEW_LINE);
    wmove(tui->buffer_win, bv->rows - bv->rows_drawn, 0);
    ti_draw_buffer_view(bv, tui->buffer_win);

    wmove(tui->status_win, 0, 0);
    werase(tui->status_win);
    /* Clear any previous background color in status window */
    wbkgd(tui->status_win, COLOR_PAIR(0));
    wprintw(tui->status_win, "Press any key to continue");
    wnoutrefresh(tui->status_win);

    doupdate();

    /* Wait for user to press any key */
    TermKeyKey key;
    termkey_waitkey(tui->termkey, &key);

    return status;
}

static Status ti_update_theme(UI *ui)
{
    TUI *tui = (TUI *)ui;
    const Theme *theme = se_get_active_theme(tui->sess);
    ThemeGroup group;

    for (size_t k = 0; k < SC_ENTRY_NUM; k++) {
        group = th_get_theme_group(theme, k);            
        init_pair(k + 1, ti_get_ncurses_color(group.fg_color), 
                         ti_get_ncurses_color(group.bg_color));
    }

    return STATUS_SUCCESS;
}

static Status ti_resize(UI *ui)
{
    TUI *tui = (TUI *)ui;

    ti_end(ui);

    struct winsize win_size;
    /* Get terminal size */
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size) == -1) {
        return st_get_error(ERR_UNABLE_TO_RESIZE_DISPLAY,
                            "Unable to determine terminal dimensions");
    }

    tui->rows = win_size.ws_row;
    tui->cols = win_size.ws_col;
    tv_resize(&tui->tv, tui->rows, tui->cols);

    resizeterm(win_size.ws_row, win_size.ws_col);

    ti_init_display(tui);
    bf_set_is_draw_dirty(tui->sess->active_buffer, 1);

    if (tui->tv.is_prompt_active) {
        assert(tui->sess->active_buffer->next != NULL);
        bf_set_is_draw_dirty(tui->sess->active_buffer->next, 1);
    }

    ti_update(ui);

    return STATUS_SUCCESS;
}

static Status ti_suspend(UI *ui)
{
    (void)ui;
    endwin();
    return STATUS_SUCCESS;
}

static Status ti_end(UI *ui)
{
    TUI *tui = (TUI *)ui;

    delwin(tui->menu_win);
    delwin(tui->buffer_win);
    delwin(tui->status_win);
    delwin(tui->line_no_win);
    endwin();

    return STATUS_SUCCESS;
}

static Status ti_free(UI *ui)
{
    TUI *tui = (TUI *)ui;

    termkey_destroy(tui->termkey);
    free(ui);

    return STATUS_SUCCESS;
}
