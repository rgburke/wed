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
#include "lib/libtermkey/termkey.h"

#define MIN_DRAW_INTERVAL_NS 200000

static void ip_handle_keypress(Session *, TermKeyKey *, char *, int *, struct timespec *, int *);
static void ip_handle_error(Session *);

static TermKey *termkey = NULL;
static int volatile window_resize_required = 0;
static sigset_t sig_set, old_set;

static void sigwinch_handler(int signal)
{
    (void)signal;
    window_resize_required = 1;
}

void ip_edit(Session *sess)
{
    termkey = termkey_new(STDIN_FILENO, TERMKEY_FLAG_SPACESYMBOL | TERMKEY_FLAG_CTRLC);

    if (termkey == NULL) {
        fatal("Unable to initialise termkey instance");
    }

    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = sigwinch_handler;

    if (sigaction(SIGWINCH, &sig_action, NULL) == -1) {
        termkey_destroy(termkey);
        fatal("Unable to set SIGWINCH signal handler");
    }

    sigemptyset(&sig_set);
    sigemptyset(&old_set);
    sigaddset(&sig_set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &sig_set, NULL);

    init_display(se_get_active_theme(sess));
    init_all_window_info(sess);
    ip_handle_error(sess);
    update_display(sess);

    ip_process_input(sess);

    end_display();
    termkey_destroy(termkey);
}

void ip_process_input(Session *sess)
{
    char keystr[MAX_KEY_STR_SIZE];
    TermKeyResult ret;
    TermKeyKey key;
    int pselect_res;
    int finished = 0;
    int redraw_due = 0;
    struct timespec last_draw; 
    struct timespec *timeout = NULL;
    struct timespec wait_timeout;
    memset(&wait_timeout, 0, sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, &last_draw);
    fd_set fds;

    while (!finished) {
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

        pselect_res = pselect(1, &fds, NULL, NULL, timeout, &old_set);

        if (pselect_res == -1) {
            if (errno == EINTR) {
                /* TODO Need to add SIGTERM handler for a graceful exit */
                if (window_resize_required) {
                    resize_display(sess);
                    window_resize_required = 0;
                    continue;
                }
            }
            /* TODO Handle general failure of pselect */
        } else if (pselect_res == 0) {
            if (termkey_getkey_force(termkey, &key) == TERMKEY_RES_KEY) {
                ip_handle_keypress(sess, &key, keystr, &finished, 
                                   &last_draw, &redraw_due);
            } else if (redraw_due) {
                update_display(sess);
                redraw_due = 0;
            }

            timeout = NULL;
        } else if (pselect_res > 0) {
            if (FD_ISSET(STDIN_FILENO, &fds)) {
                termkey_advisereadable(termkey);
            }

            while ((ret = termkey_getkey(termkey, &key)) == TERMKEY_RES_KEY) {
                ip_handle_keypress(sess, &key, keystr, &finished,
                                   &last_draw, &redraw_due);
            }

            if (ret == TERMKEY_RES_AGAIN) {
                timeout = &wait_timeout;
                timeout->tv_nsec = termkey_get_waittime(termkey) * 1000;
            } else if (ret == TERMKEY_RES_EOF) {
                finished = 1;
            }
        }

        if (redraw_due && timeout == NULL) {
            timeout = &wait_timeout;
            timeout->tv_nsec = MIN_DRAW_INTERVAL_NS;
        }
    }
}

static void ip_handle_keypress(Session *sess, TermKeyKey *key, char *keystr, 
                               int *finished, struct timespec *last_draw, int *redraw_due)
{
    static struct timespec now;
    termkey_strfkey(termkey, keystr, MAX_KEY_STR_SIZE, key, TERMKEY_FORMAT_VIM);
    se_add_error(sess, cm_do_operation(sess, keystr, finished));
    ip_handle_error(sess);
    se_save_key(sess, keystr);

    if (!*finished) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (now.tv_nsec - last_draw->tv_nsec >= MIN_DRAW_INTERVAL_NS) {
            update_display(sess);
            clock_gettime(CLOCK_MONOTONIC, last_draw);
        } else {
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
    termkey_waitkey(termkey, &key);
    se_clear_errors(sess);
}
