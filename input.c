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

#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ncurses.h>
#include "input.h"
#include "session.h"
#include "display.h"
#include "buffer.h"
#include "command.h"
#include "status.h"
#include "util.h"

/* Limit the rate at which the screen is updated. At least
 * MIN_DRAW_INTERVAL_NS nano seconds must pass between
 * screen redraws */
#define MIN_DRAW_INTERVAL_NS 200000

static void ip_setup_signal_handlers(void);
static void ip_handle_keypress(Session *, TermKeyKey *key, char *keystr,
                               int *finished, struct timespec *last_draw,
                               int *redraw_due);
static void ip_handle_error(Session *);
static int ip_is_special_key(const TermKeyKey *);

static volatile int ip_window_resize_required = 0;
static volatile int ip_continue_signal = 0;
static volatile int ip_sigterm_signal = 0;

static void ip_sigwinch_handler(int signal)
{
    (void)signal;
    ip_window_resize_required = 1;
}

static void ip_sigcont_handler(int signal)
{
    (void)signal;
    ip_continue_signal = 1;
}

static void ip_sigterm_handler(int signal)
{
    ip_sigterm_signal = signal;
}

int ip_init(InputHandler *input_handler)
{
    memset(input_handler, 0, sizeof(InputHandler));
    /* Create new termkey instance monitoring stdin with
     * the SIGINT behaviour of Ctrl-C disabled */
    input_handler->termkey = termkey_new(STDIN_FILENO,
                                TERMKEY_FLAG_SPACESYMBOL | TERMKEY_FLAG_CTRLC);

    if (input_handler->termkey == NULL) {
        warn("Unable to initialise termkey instance");
        return 0;
    }

    /* Represent ASCII DEL character as backspace */
    termkey_set_canonflags(input_handler->termkey,
                           TERMKEY_CANON_DELBS | TERMKEY_CANON_SPACESYMBOL);

    input_handler->input_type = IT_FD;

    return 1;
}

void ip_free(InputHandler *input_handler)
{
    termkey_destroy(input_handler->termkey);
}

void ip_set_keystr_input(InputHandler *input_handler, const char *keystr)
{
    assert(!is_null_or_empty(keystr));

    input_handler->keystr_input = keystr;
    input_handler->iter = keystr;
    input_handler->input_type = IT_KEYSTR;
}

void ip_set_fd_input(InputHandler *input_handler)
{
    input_handler->input_type = IT_FD;
}

static void ip_setup_signal_handlers(void)
{
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sigfillset(&sig_action.sa_mask);
    sig_action.sa_handler = ip_sigwinch_handler;

    /* Detect window size change */
    if (sigaction(SIGWINCH, &sig_action, NULL) == -1) {
        fatal("Unable to set SIGWINCH signal handler");
    }
    
    sig_action.sa_handler = ip_sigcont_handler;

    if (sigaction(SIGCONT, &sig_action, NULL) == -1) {
        fatal("Unable to set SIGCONT signal handler");
    }

    sig_action.sa_handler = ip_sigterm_handler;

    if (sigaction(SIGTERM, &sig_action, NULL) == -1) {
        fatal("Unable to set SIGTERM signal handler");
    }

    if (sigaction(SIGHUP, &sig_action, NULL) == -1) {
        fatal("Unable to set SIGHUP signal handler");
    }

    if (sigaction(SIGINT, &sig_action, NULL) == -1) {
        fatal("Unable to set SIGINT signal handler");
    }

    sigset_t sig_set;
    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGWINCH);
    sigaddset(&sig_set, SIGCONT);
    sigaddset(&sig_set, SIGTERM);
    sigaddset(&sig_set, SIGHUP);
    sigaddset(&sig_set, SIGINT);
    sigprocmask(SIG_BLOCK, &sig_set, NULL);
}

void ip_edit(Session *sess)
{
    ip_setup_signal_handlers();

    init_display(se_get_active_theme(sess));
    init_all_window_info(sess);

    if (sess->input_handler.input_type == IT_KEYSTR) {
        ip_process_keystr_input(sess);
        ip_set_fd_input(&sess->input_handler);
    }

    /* If there were errors parsing config
     * or initialising the session then display
     * them first */
    ip_handle_error(sess);
    update_display(sess);

    ip_process_input(sess);

    end_display();
}

void ip_process_input(Session *sess)
{
    if (sess->input_handler.input_type == IT_KEYSTR) {
        ip_process_keystr_input(sess);
        return;
    }

    TermKey *termkey = sess->input_handler.termkey;
    char keystr[MAX_KEY_STR_SIZE];
    TermKeyResult ret;
    TermKeyKey key;
    int pselect_res;
    int finished = 0;
    int redraw_due = 0;
    struct timespec last_draw; 
    struct timespec *timeout = NULL;
    struct timespec wait_timeout;
    static sigset_t old_set;
    memset(&wait_timeout, 0, sizeof(struct timespec));
    /* old_set is used in pselect to control
     * when SIGWINCH signal fires */
    sigemptyset(&old_set);
    /* Use monotonic clock as we're only interested in
     * measuring time intervals that have passed */
    clock_gettime(CLOCK_MONOTONIC, &last_draw);
    fd_set fds;

    while (!finished) {
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

        /* Wait for user input or signal */
        pselect_res = pselect(1, &fds, NULL, NULL, timeout, &old_set);

        if (pselect_res == -1) {
        /* pselect failed */
            if (errno == EINTR) {
                /* A signal was caught */
                if (ip_window_resize_required) {
                    resize_display(sess);
                    ip_window_resize_required = 0;
                    continue;
                } else if (ip_continue_signal) {
                    resize_display(sess);
                    ip_continue_signal = 0;
                    continue;
                } else if (ip_sigterm_signal) {
                    end_display();
                    termkey_destroy(sess->input_handler.termkey);
                    exit(ip_sigterm_signal);
                }
            }
            /* TODO Handle general failure of pselect */
        } else if (pselect_res == 0) {
            /* pselect timed out so attempt to interpret any unprocessed
             * input as a key */
            if (termkey_getkey_force(termkey, &key) == TERMKEY_RES_KEY) {
                ip_handle_keypress(sess, &key, keystr, &finished, 
                                   &last_draw, &redraw_due);
            } else if (redraw_due) {
                /* A redraw is due and there has been no further input from
                 * the user so update the display */
                update_display(sess);
                redraw_due = 0;
            }

            timeout = NULL;
        } else if (pselect_res > 0) {
            if (FD_ISSET(STDIN_FILENO, &fds)) {
                /* Inform termkey input is available to be read */
                termkey_advisereadable(termkey);
            }

            while ((ret = termkey_getkey(termkey, &key)) == TERMKEY_RES_KEY) {
                ip_handle_keypress(sess, &key, keystr, &finished,
                                   &last_draw, &redraw_due);
            }

            if (ret == TERMKEY_RES_AGAIN) {
                /* Partial keypress found, try waiting for more input */
                timeout = &wait_timeout;
                timeout->tv_nsec = termkey_get_waittime(termkey) * 1000;
            } else if (ret == TERMKEY_RES_EOF) {
                finished = 1;
            }
        }

        if (redraw_due && timeout == NULL) {
            /* A redraw is pending so set timeout so MIN_DRAW_INTERVAL_NS.
             * If no further input occurs then pselect will timeout and
             * the screen will be redrawn */
            timeout = &wait_timeout;
            timeout->tv_nsec = MIN_DRAW_INTERVAL_NS;
        }
    }
}

static void ip_handle_keypress(Session *sess, TermKeyKey *key, char *keystr,
                               int *finished, struct timespec *last_draw,
                               int *redraw_due)
{
    static struct timespec now;
    termkey_strfkey(sess->input_handler.termkey, keystr,
                    MAX_KEY_STR_SIZE,key, TERMKEY_FORMAT_VIM);
    /* This is where user input invokes a command */
    se_add_error(sess, cm_do_operation(sess, keystr, finished));
    /* Immediately display any errors that have occurred */
    ip_handle_error(sess);
    se_save_key(sess, keystr);

    if (!*finished) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (now.tv_nsec - last_draw->tv_nsec >= MIN_DRAW_INTERVAL_NS) {
            update_display(sess);
            clock_gettime(CLOCK_MONOTONIC, last_draw);
        } else {
            /* A redraw is due but wait longer to see if the user enters
             * more input before refreshing screen. This allows us to deal
             * with a user pasting a large amount of text into the terminal
             * smoothly */
            *redraw_due = 1;
        }
    }
}

static void ip_handle_error(Session *sess)
{
    if (!se_has_errors(sess)) {
        return;
    }

    TermKeyKey key;
    draw_errors(sess);
    /* Wait for user to press any key */
    termkey_waitkey(sess->input_handler.termkey, &key);
    se_clear_errors(sess);
}

void ip_process_keystr_input(Session *sess)
{
    InputHandler *input_handler = &sess->input_handler;
    TermKey *termkey = input_handler->termkey;

    assert(input_handler->keystr_input != NULL);
    assert(input_handler->iter != NULL);

    char keystr[MAX_KEY_STR_SIZE];
    int finished = 0;
    TermKeyKey key;
    Status status;
    const char *next;

    while (*input_handler->iter && !finished) {
        if (*input_handler->iter == '<' &&
            (next = termkey_strpkey(termkey, input_handler->iter + 1,
                                    &key, TERMKEY_FORMAT_VIM)
            ) != NULL && ip_is_special_key(&key) && *next == '>') {
            /* Key has string representation of the form <...> */
            termkey_strfkey(input_handler->termkey, keystr, MAX_KEY_STR_SIZE,
                            &key, TERMKEY_FORMAT_VIM);
            input_handler->iter = next + 1;
        } else if ((next = termkey_strpkey(termkey, input_handler->iter,
                                           &key, TERMKEY_FORMAT_VIM)
                   ) != NULL) {
            /* Normal Unicode key */
            termkey_strfkey(input_handler->termkey, keystr, MAX_KEY_STR_SIZE,
                            &key, TERMKEY_FORMAT_VIM);
            input_handler->iter = next;
        } else {
            se_add_error(sess, st_get_error(ERR_INVALID_KEY,
                                            "Invalid key specified in key "
                                            "string starting from %s",
                                            input_handler->iter)); 
            return;
        }

        status = cm_do_operation(sess, keystr, &finished);

        if (!STATUS_IS_SUCCESS(status)) {
            se_add_error(sess, status);

            if (sess->wed_opt.test_mode) {
                return;
            }
        }

        se_save_key(sess, keystr);
    }
}

/* Does key have string representation of the form <...>.
 * This function is used when parsing a key string
 * to distinguish between keys and strings.
 * e.g. <Tab> and <C-v> are keys whereas <b> is a string */
static int ip_is_special_key(const TermKeyKey *key)
{
    if (key->type == TERMKEY_TYPE_FUNCTION ||
        key->type == TERMKEY_TYPE_KEYSYM) {
        return 1;
    }
    
    return key->modifiers & (TERMKEY_KEYMOD_CTRL | TERMKEY_KEYMOD_ALT);
}
