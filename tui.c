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
#include "config.h"

#define DOUBLE_CLICK_TIMEFRAME_NS 500000000 
#define SC_COLOR_PAIR(screen_comp) (COLOR_PAIR((screen_comp) + 1))
#define WED_MOUSE_BUFFER_CLICK "<wed-buffer-mouse-click>"
#define WED_MOUSE_FILE_EXPLORER_CLICK "<wed-file-explorer-mouse-click>"
#define WED_MOUSE_TAB_CLICK "<wed-tab-mouse-click>"

static Status ti_init(UI *);
static void ti_init_display(UI *);
static short ti_get_ncurses_color(DrawColor);
static MouseClickType ti_get_mouse_click_type(TermKeyMouseEvent);
static int ti_convert_to_win_pos(const WINDOW *, size_t *row, size_t *col);
static int ti_convert_to_buffer_pos(WINDOW *win, const BufferView *,
                                    int *row_ptr, int *col_ptr);
static int ti_convert_to_buffer_index(const TUI *, size_t row, size_t col,
                                      size_t *buffer_index_ptr);
static int ti_events_equal(const MouseClickEvent *, const MouseClickEvent *);
static int ti_monitor_for_double_click_event(TUI *, const WINDOW *,
                                             const MouseClickEvent *);
static void ti_get_mouse_double_click_event(const TUI *, const WINDOW *,
                                            MouseClickEvent *);
static Status ti_get_input(UI *);
static Status ti_update(UI *);
static void ti_setup_window(WINDOW *, const ViewDimensions *new,
                            const ViewDimensions *old);
static void ti_draw_buffer_tabs(TUI *);
static void ti_draw_line_no(TUI *);
static void ti_draw_file_explorer(TUI *);
static void ti_draw_buffer(TUI *);
static void ti_draw_buffer_view(const BufferView *, WINDOW *);
static int ti_draw_buffer_line(WINDOW *, const BufferView *, const Line *);
static int ti_draw_buffer_cell(WINDOW *, const Cell *);
static void ti_draw_status_bar(TUI *);
static void ti_draw_prompt(TUI *);
static void ti_position_cursor(TUI *);
static Status ti_error(UI *);
static Status ti_update_theme(UI *);
static Status ti_toggle_mouse_support(UI *);
static Status ti_resize(UI *);
static Status ti_suspend(UI *);
static Status ti_resume(UI *);
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
    tui->ui.toggle_mouse_support = ti_toggle_mouse_support;
    tui->ui.resize = ti_resize;
    tui->ui.suspend = ti_suspend;
    tui->ui.resume = ti_resume;
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
        fatal("Unable to create termkey instance");
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
    ti_init_display(ui);
    refresh();

    return STATUS_SUCCESS;
}

static void ti_init_display(UI *ui)
{
    TUI *tui = (TUI *)ui;

    if (has_colors()) {
        start_color();
        use_default_colors();
        ti_update_theme(ui);
    }

    /* See ncurses man pages for
     * explanations of these functions */
    raw();
    noecho();
    nl();
    keypad(stdscr, TRUE);
    curs_set(1);
    tui->mouse_mask = ALL_MOUSE_EVENTS;

    if (cf_bool(tui->sess->config, CV_MOUSE)) {
        ti_toggle_mouse_support(ui);
    }

    tui->menu_win = newwin(1, tui->cols, 0, 0); 
    tui->buffer_win = newwin(tui->rows - 2, tui->cols, 1, 0);
    tui->status_win = newwin(0, tui->cols, tui->rows - 1, 0);
    tui->line_no_win = newwin(0, 0, 1, 0);
    tui->file_explorer_win = newwin(0, 0, 1, 0);
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

static MouseClickType ti_get_mouse_click_type(TermKeyMouseEvent event)
{
    static MouseClickType mouse_click_types[] = {
        [TERMKEY_MOUSE_PRESS - 1] = MCT_PRESS,
        [TERMKEY_MOUSE_DRAG - 1] = MCT_DRAG,
        [TERMKEY_MOUSE_RELEASE - 1] = MCT_RELEASE
    };

    static const size_t mouse_click_type_num = ARRAY_SIZE(mouse_click_types,
                                                          MouseClickType);
    assert(event > 0 && event <= mouse_click_type_num);

    return mouse_click_types[event - 1];
}

static int ti_convert_to_win_pos(const WINDOW *win, size_t *row_ptr,
                                 size_t *col_ptr)
{
    int start_row, start_col;
    getbegyx(win, start_row, start_col);
    start_row++;
    start_col++;

    int rows, cols;
    getmaxyx(win, rows, cols);

    if (*row_ptr < (size_t)start_row ||
        *row_ptr >= (size_t)(start_row + rows) ||
        *col_ptr < (size_t)start_col ||
        *col_ptr >= (size_t)(start_col + cols)) {
        return 0;
    }

    *row_ptr -= start_row;
    *col_ptr -= start_col;

    return 1;
}

static int ti_convert_to_buffer_pos(WINDOW *win, const BufferView *bv,
                                    int *row_ptr, int *col_ptr)
{
    size_t row = *row_ptr;
    size_t col = *col_ptr;

    if (!ti_convert_to_win_pos(win, &row, &col)) {
        return 0;
    }

    if (!bv_convert_screen_pos_to_buffer_pos(bv, &row, &col)) {
        return 0;
    }

    *row_ptr = row;
    *col_ptr = col;

    return 1;
}

static int ti_convert_to_buffer_index(const TUI *tui, size_t row, size_t col,
                                      size_t *buffer_index_ptr)
{
    if (!ti_convert_to_win_pos(tui->menu_win, &row, &col)) {
        return 0;
    }

    const TabbedView *tv = &tui->tv;
    size_t start_col = 0;
    size_t tab_length;
    size_t buffer_index;

    for (buffer_index = 0; buffer_index < tv->buffer_tab_num; buffer_index++) {
       tab_length = strlen(tv->buffer_tabs[buffer_index])
                    + strlen(tv->tab_separator);

       if (col >= start_col && col < start_col + tab_length) {
           break;
       }

       start_col += tab_length;
    }

    if (buffer_index == tv->buffer_tab_num) {
        buffer_index--;
    }

    *buffer_index_ptr = tv->first_buffer_tab_index + buffer_index;

    return 1;
}

static int ti_events_equal(const MouseClickEvent *e1, const MouseClickEvent *e2)
{
    if (e1->event_type != e2->event_type ||
        e1->click_type != e2->click_type) {
        return 0;
    }

    if (e1->event_type == MCET_BUFFER) {
        if (e1->data.click_pos.row != e2->data.click_pos.row ||
            e1->data.click_pos.col != e2->data.click_pos.col) {
            return 0;
        }
    } else if (e1->event_type == MCET_TAB) {
        if (e1->data.buffer_index != e2->data.buffer_index) {
            return 0;
        }
    } else {
        return 0;
    }

    return 1;
}

static int ti_monitor_for_double_click_event(TUI *tui, const WINDOW *click_win,
                                             const MouseClickEvent *event)
{
    int double_click_detected = 0;
    DoubleClickMonitor *double_click_monitor = &tui->double_click_monitor;
    MouseClickEvent *last_event = &double_click_monitor->last_mouse_press;
    struct timespec time;
    memset(&time, 0, sizeof(struct timespec));

    if (double_click_monitor->click_win == click_win &&
        ti_events_equal(last_event, event)) {
        get_monotonic_time(&time);
        const struct timespec *last_mouse_press_time =
            &double_click_monitor->last_mouse_press_time;
         
        if (time.tv_sec - last_mouse_press_time->tv_sec == 0 &&
            time.tv_nsec - last_mouse_press_time->tv_nsec
                <= DOUBLE_CLICK_TIMEFRAME_NS) {
            double_click_detected = 1;
        }
    }

    if (event->event_type == MCET_BUFFER && event->click_type == MCT_PRESS) {
        *last_event = *event;

        if (time.tv_nsec == 0) {
            get_monotonic_time(&time);
        }

        double_click_monitor->last_mouse_press_time = time;
        double_click_monitor->click_win = click_win;
    }

    return double_click_detected;
}

static void ti_get_mouse_double_click_event(const TUI *tui,
                                            const WINDOW *click_win,
                                            MouseClickEvent *event)
{
    if (click_win == tui->file_explorer_win) {
        event->click_type = MCT_DOUBLE_PRESS;
    }
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
    Status status = STATUS_SUCCESS;

    if (input_buffer->arg == IA_INPUT_AVAILABLE_TO_READ) {
        /* Inform termkey input is available to be read */
        termkey_advisereadable(termkey);

        while ((ret = termkey_getkey(termkey, &key)) == TERMKEY_RES_KEY) {
            keystr_len = termkey_strfkey(termkey, keystr, MAX_KEY_STR_SIZE,
                                         &key, TERMKEY_FORMAT_VIM);

            if (key.type == TERMKEY_TYPE_MOUSE) {
                TermKeyMouseEvent event;
                int row, col;

                termkey_interpret_mouse(termkey, &key, &event, NULL,
                                        &row, &col);

                if (event != TERMKEY_MOUSE_UNKNOWN) {
                    size_t buffer_index;
                    const WINDOW *click_win = NULL;
                    const char *buffer_click_event = NULL;

                    if (ti_convert_to_buffer_pos(tui->buffer_win, tui->tv.bv,
                                                 &row, &col)) {
                        buffer_click_event = WED_MOUSE_BUFFER_CLICK;
                        click_win = tui->buffer_win;
                    } else if (ti_convert_to_buffer_pos(tui->file_explorer_win,
                        tui->sess->file_explorer->buffer->bv, &row, &col)) {
                        buffer_click_event = WED_MOUSE_FILE_EXPLORER_CLICK;
                        click_win = tui->file_explorer_win;
                    }

                    if (buffer_click_event != NULL) {
                        MouseClickEvent mouse_click_event = {
                            .event_type = MCET_BUFFER,
                            .click_type = ti_get_mouse_click_type(event),
                            .data = {
                                .click_pos = {
                                    .row = row,
                                    .col = col
                                }
                            }
                        };

                        if (ti_monitor_for_double_click_event(tui, click_win,
                                    &mouse_click_event)) {
                            ti_get_mouse_double_click_event(tui, click_win,
                                    &mouse_click_event);
                        }

                        status = ip_add_mouse_click_event(input_buffer,
                                buffer_click_event,
                                strlen(buffer_click_event),
                                &mouse_click_event);
                    } else if (ti_convert_to_buffer_index(tui, row, col,
                                                          &buffer_index)) {
                        MouseClickEvent mouse_click_event = {
                            .event_type = MCET_TAB,
                            .click_type = ti_get_mouse_click_type(event),
                            .data = {
                                .buffer_index = buffer_index
                            }
                        };

                        status = ip_add_mouse_click_event(input_buffer,
                                WED_MOUSE_TAB_CLICK,
                                strlen(WED_MOUSE_TAB_CLICK),
                                &mouse_click_event);
                    }
                }
            } else {
                status = ip_add_keystr_input_to_end(input_buffer, keystr,
                                                    keystr_len);
            }

            RETURN_IF_FAIL(status);
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

    return status;
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
        ti_draw_buffer(tui);
        ti_draw_line_no(tui);
        ti_draw_file_explorer(tui);
        ti_draw_status_bar(tui);
    }

    ti_position_cursor(tui);

    doupdate();

    return STATUS_SUCCESS;
}

static void ti_setup_window(WINDOW *win, const ViewDimensions *new,
                            const ViewDimensions *old)
{
    int width_diff = new->cols - old->cols;
    int start_diff = new->start_col != old->start_col;

    if (!width_diff && !start_diff) {
        return;
    }

    werase(win);

    if (width_diff > 0) {
        mvwin(win, new->start_row, new->start_col);
        start_diff = 0;
    }

    wresize(win, new->rows, new->cols);

    if (width_diff < 0 || start_diff) {
        mvwin(win, new->start_row, new->start_col);
    }

    werase(win);
}

static void ti_draw_buffer_tabs(TUI *tui)
{
    const TabbedView *tv = &tui->tv;

    ti_setup_window(tui->menu_win, &tv->vd.buffer_tab,
                    &tv->last_vd.buffer_tab);
    
    assert(tui->sess->active_buffer_index >= tui->tv.first_buffer_tab_index);
    size_t active_buffer_index = tui->sess->active_buffer_index -
                                 tui->tv.first_buffer_tab_index;
    assert(active_buffer_index < tv->buffer_tab_num);

    wmove(tui->menu_win, 0, 0);
    wbkgd(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    wattron(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));
    
    const size_t tab_separator_num = tv->buffer_tab_num - 1;
    size_t tab_separator_positions[tab_separator_num];

    for (size_t k = 0; k < tv->buffer_tab_num; k++) {
        if (k == active_buffer_index) {
            /* The active tab has custom coloring */
            wattron(tui->menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
            waddstr(tui->menu_win, tv->buffer_tabs[k]); 
            wattroff(tui->menu_win, SC_COLOR_PAIR(SC_ACTIVE_BUFFER_TAB_BAR));
        } else {
            waddstr(tui->menu_win, tv->buffer_tabs[k]); 
        } 

        if (k < tab_separator_num) {
            int y, x;
            (void)y;
            getyx(tui->menu_win, y, x);
            tab_separator_positions[k] = x;
            waddstr(tui->menu_win, tv->tab_separator);
        }
    }

    wclrtoeol(tui->menu_win);
    wattroff(tui->menu_win, SC_COLOR_PAIR(SC_BUFFER_TAB_BAR));

    for (size_t k = 0; k < tab_separator_num; k++) {
        mvwvline(tui->menu_win, 0, tab_separator_positions[k], ACS_VLINE, 1);
    }

    wnoutrefresh(tui->menu_win); 
}

static void ti_draw_line_no(TUI *tui)
{
    const TabbedView *tv = &tui->tv;
    const BufferView *bv = tv->bv;

    ti_setup_window(tui->line_no_win, &tv->vd.line_no, &tv->last_vd.line_no);

    size_t cols = tv->vd.line_no.cols;
    size_t rows = tv->vd.line_no.rows;

    if (cols == 0) {
        return;
    }

    const Line *line;

    for (size_t row = 0; row < rows; row++) {
        line = &bv->lines[row];
        wmove(tui->line_no_win, row, 0);

        if (line->line_no != 0) {
            wattron(tui->line_no_win, SC_COLOR_PAIR(SC_LINENO));
            wprintw(tui->line_no_win, "%*zu ", ((int)cols - 1), line->line_no);
            wattroff(tui->line_no_win, SC_COLOR_PAIR(SC_LINENO));
        } else {
            wprintw(tui->line_no_win, "%*s ", ((int)cols - 1), "");
        }
    }

    mvwvline(tui->line_no_win, 0, cols - 1, ACS_VLINE, rows);

    wnoutrefresh(tui->line_no_win); 
}

static void ti_draw_file_explorer(TUI *tui)
{
    const TabbedView *tv = &tui->tv;

    ti_setup_window(tui->file_explorer_win, &tv->vd.file_explorer,
                    &tv->last_vd.file_explorer);

    const size_t cols = tv->vd.file_explorer.cols;

    if (cols == 0) {
        return;
    }

    const Session *sess = tui->sess;
    const FileExplorer *file_explorer = sess->file_explorer;
    const size_t file_explorer_width = cf_int(sess->config,
                                              CV_FILE_EXPLORER_WIDTH);
    const size_t title_len = strlen(tv->file_explorer_title);
    const size_t title_start_x =
        ((file_explorer_width - 3 - title_len) / 2) + 1;

    wmove(tui->file_explorer_win, 0, 0);
    wclrtoeol(tui->file_explorer_win);
    wattron(tui->file_explorer_win, SC_COLOR_PAIR(SC_FILE_EXPLORER_TITLE));
    wmove(tui->file_explorer_win, 0, title_start_x);
    wprintw(tui->file_explorer_win, tv->file_explorer_title);
    wattroff(tui->file_explorer_win, SC_COLOR_PAIR(SC_FILE_EXPLORER_TITLE));

    const Buffer *buffer = fe_get_buffer(file_explorer);
    const BufferView *bv = buffer->bv;
    const size_t rows = tv->vd.file_explorer.rows;
    const size_t dir_entries = file_explorer->dir_entries;

    wmove(tui->file_explorer_win, 1, 0);
    wattron(tui->file_explorer_win,
            SC_COLOR_PAIR(SC_FILE_EXPLORER_FILE_ENTRY));
    ti_draw_buffer_view(bv, tui->file_explorer_win);
    wattroff(tui->file_explorer_win,
             SC_COLOR_PAIR(SC_FILE_EXPLORER_FILE_ENTRY));

    const size_t visible_dir_entries =
        bv->screen_start.line_no <= dir_entries ?
        dir_entries - (bv->screen_start.line_no - 1) : 0;

    for (size_t row = 0; row < visible_dir_entries; row++) {
        mvwchgat(tui->file_explorer_win, row + 1, 0, cols - 1, A_NORMAL,
                 SC_FILE_EXPLORER_DIRECTORY_ENTRY + 1, NULL);
    }

    size_t selected_line_offset =
        buffer->pos.line_no - bv->screen_start.line_no;
    size_t selected_colour_pair =
        selected_line_offset < visible_dir_entries ?
        SC_FILE_EXPLORER_DIRECTORY_ENTRY + 1 : 1;

    attr_t selected_attr = A_REVERSE;

    if (!tv->is_file_explorer_active) {
        selected_attr |= A_DIM;
    }

    mvwchgat(tui->file_explorer_win, selected_line_offset + 1, 0, cols - 1,
             selected_attr, selected_colour_pair, NULL);

    mvwvline(tui->file_explorer_win, 0, cols - 1, ACS_VLINE, rows);

    wnoutrefresh(tui->file_explorer_win); 
}

static void ti_draw_buffer(TUI *tui)
{
    const TabbedView *tv = &tui->tv;

    ti_setup_window(tui->buffer_win, &tv->vd.buffer, &tv->last_vd.buffer);

    wmove(tui->buffer_win, 0, 0);
    ti_draw_buffer_view(tv->bv, tui->buffer_win);
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
    mvwprintw(tui->status_win, 0,
              tv->vd.status_bar.cols - pos_info_len - 1,
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
    const TabbedView *tv = &tui->tv;

    if (tv->is_file_explorer_active) {
        curs_set(0);
        return;
    } else {
        curs_set(1);
    }

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

static Status ti_toggle_mouse_support(UI *ui)
{
    TUI *tui = (TUI *)ui;

    mousemask(tui->mouse_mask, &tui->mouse_mask);

    return STATUS_SUCCESS;
}

static Status ti_resize(UI *ui)
{
    TUI *tui = (TUI *)ui;
    Session *sess = tui->sess;

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

    ti_init_display(ui);
    bf_set_is_draw_dirty(tui->sess->active_buffer, 1);

    if (pr_get_prompt_buffer(sess->prompt) == sess->active_buffer) {
        assert(tui->sess->active_buffer->next != NULL);
        bf_set_is_draw_dirty(tui->sess->active_buffer->next, 1);
    }

    ti_update(ui);

    return STATUS_SUCCESS;
}

static Status ti_suspend(UI *ui)
{
    TUI *tui = (TUI *)ui;
    endwin();
    termkey_stop(tui->termkey);

    return STATUS_SUCCESS;
}

static Status ti_resume(UI *ui)
{
    TUI *tui = (TUI *)ui;
    termkey_start(tui->termkey);
    def_shell_mode();
    refresh();

    return ti_resize(ui);
}

static Status ti_end(UI *ui)
{
    TUI *tui = (TUI *)ui;

    delwin(tui->menu_win);
    delwin(tui->buffer_win);
    delwin(tui->status_win);
    delwin(tui->line_no_win);
    delwin(tui->file_explorer_win);
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
