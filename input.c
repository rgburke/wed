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

#include <errno.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <ncurses.h>
#include "input.h"
#include "session.h"
#include "buffer.h"
#include "command.h"
#include "status.h"
#include "util.h"
#include "tui.h"

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

/* Limit the rate at which the screen is updated. At least
 * MIN_DRAW_INTERVAL_NS nano seconds must pass between
 * screen redraws */
#define MIN_DRAW_INTERVAL_NS 200000

static Status ip_add_keystr_input(InputBuffer *, size_t pos,
                                  const char *keystr, size_t keystr_len);
static int ip_input_available(const InputBuffer *);
static void ip_setup_signal_handlers(void);
static void ip_process_input_buffer(Session *, int *finished,
                                    struct timespec *last_draw,
                                    int *redraw_due);
static Status ip_get_next_key(Session *, GapBuffer *, char *keystr_buffer,
                              size_t keystr_buffer_len, size_t *keystr_len);
static int ip_parse_key(const Session *, const char *keystr,
                        char *keystr_buffer, size_t keystr_buffer_len,
                        size_t *keystr_len, size_t *parsed_len);
static void ip_handle_keypress(Session *, const char *keystr, int *finished,
                               struct timespec *last_draw, int *redraw_due);
static void ip_handle_error(Session *);
static int ip_is_special_key(const TermKeyKey *);
static int ip_is_wed_operation(const char *key, const char **next);

static volatile int ip_window_resize_required = 0;
static volatile int ip_continue_signal = 0;
static volatile int ip_sigterm_signal = 0;
static void ip_get_monotonic_time(struct timespec *);

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

int ip_init(InputBuffer *input_buffer)
{
    memset(input_buffer, 0, sizeof(InputBuffer));

    input_buffer->buffer = gb_new(1024);

    if (input_buffer->buffer == NULL) {
        return 0;
    }

    return 1;
}

void ip_free(InputBuffer *input_buffer)
{
    gb_free(input_buffer->buffer);
}

Status ip_add_keystr_input_to_end(InputBuffer *input_buffer,
                                  const char *keystr, size_t keystr_len)
{
    return ip_add_keystr_input(input_buffer, gb_length(input_buffer->buffer),
                               keystr, keystr_len);
}

Status ip_add_keystr_input_to_start(InputBuffer *input_buffer,
                                    const char *keystr, size_t keystr_len)
{
    return ip_add_keystr_input(input_buffer, 0, keystr, keystr_len);
}

static Status ip_add_keystr_input(InputBuffer *input_buffer, size_t pos,
                                  const char *keystr, size_t keystr_len)
{
    assert(!is_null_or_empty(keystr));
    GapBuffer *buffer = input_buffer->buffer;

    gb_set_point(buffer, pos);
     
    if (!gb_add(buffer, keystr, keystr_len)) {
        return OUT_OF_MEMORY("Unable to save input");
    }

    gb_set_point(buffer, 0);

    return STATUS_SUCCESS;
}

static int ip_input_available(const InputBuffer *input_buffer)
{
    GapBuffer *buffer = input_buffer->buffer;
    return (gb_length(buffer) - gb_get_point(buffer)) > 0;
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

    sess->ui->init(sess->ui);

    /* If there were errors parsing config
     * or initialising the session then display
     * them first */
    if (se_has_errors(sess)) {
        sess->ui->update(sess->ui);
        ip_handle_error(sess);
    }

    sess->ui->update(sess->ui);

    ip_process_input(sess);

    sess->ui->end(sess->ui);
}

void ip_process_input(Session *sess)
{
    InputBuffer *input_buffer = &sess->input_buffer;
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
    ip_get_monotonic_time(&last_draw);
    fd_set fds;

    if (sess->wed_opt.test_mode) {
        ip_process_input_buffer(sess, &finished, &last_draw, &redraw_due);
        return;
    }

    while (!finished) {
        if (ip_input_available(&sess->input_buffer)) {
            ip_process_input_buffer(sess, &finished, &last_draw, &redraw_due);
        } else {
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);

            /* Wait for user input or signal */
            pselect_res = pselect(1, &fds, NULL, NULL, timeout, &old_set);

            if (pselect_res == -1) {
                /* pselect failed */
                if (errno == EINTR) {
                    /* A signal was caught */
                    if (ip_window_resize_required) {
                        sess->ui->resize(sess->ui);
                        ip_window_resize_required = 0;
                        continue;
                    } else if (ip_continue_signal) {
                        sess->ui->resize(sess->ui);
                        ip_continue_signal = 0;
                        continue;
                    } else if (ip_sigterm_signal) {
                        sess->ui->end(sess->ui);
                        exit(ip_sigterm_signal);
                    }
                }
                /* TODO Handle general failure of pselect */
            } else if (pselect_res == 0) {
                input_buffer->arg = IA_NO_INPUT_AVAILABLE_TO_READ;
                sess->ui->get_input(sess->ui);
                /* pselect timed out so attempt to interpret any unprocessed
                 * input as a key */
                if (input_buffer->result == IR_INPUT_ADDED) {
                    ip_process_input_buffer(sess, &finished, &last_draw,
                            &redraw_due);
                } else if (redraw_due) {
                    /* A redraw is due and there has been no further input from
                     * the user so update the display */
                    sess->ui->update(sess->ui);
                    redraw_due = 0;
                }

                timeout = NULL;
            } else if (pselect_res > 0) {
                input_buffer->arg = IA_INPUT_AVAILABLE_TO_READ;
                sess->ui->get_input(sess->ui);

                if (input_buffer->result == IR_WAIT_FOR_MORE_INPUT) {
                    timeout = &wait_timeout;
                    timeout->tv_nsec = input_buffer->wait_time_nano;
                } else if (input_buffer->result == IR_EOF) {
                    finished = 1;
                } else if (input_buffer->result == IR_INPUT_ADDED) {
                    ip_process_input_buffer(sess, &finished, &last_draw,
                            &redraw_due);
                }
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

static void ip_process_input_buffer(Session *sess, int *finished,
                                    struct timespec *last_draw,
                                    int *redraw_due)
{
    InputBuffer *input_buffer = &sess->input_buffer;
    GapBuffer *buffer = input_buffer->buffer;

    static char keystr[MAX_KEY_STR_SIZE];
    size_t keystr_len;
    Status status;

    while (ip_input_available(input_buffer) && !*finished) {
        status = ip_get_next_key(sess, buffer, keystr,
                                 sizeof(keystr), &keystr_len);

        if (STATUS_IS_SUCCESS(status)) {
            ip_handle_keypress(sess, keystr, finished,
                               last_draw, redraw_due);
        } else {
            se_add_error(sess, status);
            ip_handle_error(sess);
        }

        if (sess->wed_opt.test_mode && se_has_errors(sess)) {
            gb_clear(sess->input_buffer.buffer);
            return;
        }
    }
}

static Status ip_get_next_key(Session *sess, GapBuffer *buffer,
                              char *keystr_buffer, size_t keystr_buffer_len,
                              size_t *keystr_len_ptr)
{
    char keystr[keystr_buffer_len];
    size_t bytes = gb_get_range(buffer, 0, keystr,
                                MIN(gb_length(buffer), keystr_buffer_len - 1));
    assert(bytes > 0);
    keystr[bytes] = '\0';
    keystr_buffer[0] = '\0';

    Status status = STATUS_SUCCESS;
    size_t total_parsed_len = 0;
    int is_prefix = 0;
    int is_valid = 0;
    size_t keys = 0;
    size_t keystr_len = 0;

    size_t key_len = 0;
    size_t parsed_len = 0;

    do {
        if (!ip_parse_key(sess, keystr + total_parsed_len,
                          keystr_buffer + keystr_len,
                          keystr_buffer_len, &key_len, &parsed_len)) {
            status = st_get_error(ERR_INVALID_KEY, "Invalid key specified "
                                  "starting from %s",
                                  keystr + total_parsed_len); 
        } else {
            keystr_len += key_len;
            total_parsed_len += parsed_len;
            is_valid = cm_is_valid_operation(sess, keystr_buffer,
                                             keystr_len, &is_prefix);
        }

        keys++;
    } while (STATUS_IS_SUCCESS(status) && is_prefix && !is_valid &&
             total_parsed_len < bytes);

    if (!STATUS_IS_SUCCESS(status)) {
        if (keys > 1) {
            int valid = ip_parse_key(sess, keystr, keystr_buffer,
                                     keystr_buffer_len, &keystr_len,
                                     &total_parsed_len);
            assert(valid);
            status = STATUS_SUCCESS;
        } else {
            total_parsed_len = 1;
        }
    } else if (!is_valid) {
        if (is_prefix) {
            keystr_len = 0;
        } else if (keys > 1) {
            keystr_len -= key_len;
            total_parsed_len -= parsed_len;
            keystr_buffer[keystr_len] = '\0';
        }
    }

    *keystr_len_ptr = keystr_len;

    if (is_prefix && keystr_len == 0) {
        gb_set_point(buffer, total_parsed_len);
    } else {
        gb_set_point(buffer, 0);
        gb_delete(buffer, total_parsed_len);
    }

    return status;
}

static int ip_parse_key(const Session *sess, const char *keystr,
                        char *keystr_buffer, size_t keystr_buffer_len,
                        size_t *keystr_len, size_t *parsed_len)
{
    /* TODO Need to move all termkey related functionality behind UI
     * interface */
    TermKey *termkey = ((TUI *)sess->ui)->termkey;
    TermKeyKey key;

    const char *iter = keystr;
    const char *next;

    if (*iter == '<' && ip_is_wed_operation(iter, &next)) {
        *keystr_len = *parsed_len = next - iter;
        assert(*keystr_len < keystr_buffer_len);
        memcpy(keystr_buffer, iter, MIN(*keystr_len, keystr_buffer_len)); 
        keystr_buffer[*keystr_len] = '\0';
    } else if (*iter == '<' && 
               (next = termkey_strpkey(termkey, iter + 1, &key,
                                       TERMKEY_FORMAT_VIM)) != NULL &&
               ip_is_special_key(&key) && *next == '>') {
        /* Key has string representation of the form <...> */
        termkey_strfkey(termkey, keystr_buffer, keystr_buffer_len,
                        &key, TERMKEY_FORMAT_VIM);
        *keystr_len = *parsed_len = strlen(keystr_buffer);
    } else if ((next = termkey_strpkey(termkey, iter, &key,
                                       TERMKEY_FORMAT_VIM)) != NULL) {
        /* Normal Unicode key */
        termkey_strfkey(termkey, keystr_buffer, keystr_buffer_len, &key,
                        TERMKEY_FORMAT_VIM);

        *parsed_len = next - iter;
        *keystr_len = strlen(keystr_buffer);
    } else {
        return 0;
    }

    return 1;
}

static void ip_handle_keypress(Session *sess, const char *keystr,
                               int *finished, struct timespec *last_draw,
                               int *redraw_due)
{
    static struct timespec now;
    /* This is where user input invokes a command */
    se_add_error(sess, cm_do_operation(sess, keystr, finished));
    /* Immediately display any errors that have occurred */
    ip_handle_error(sess);
    se_save_key(sess, keystr);

    if (!*finished) {
        ip_get_monotonic_time(&now);

        if (now.tv_nsec - last_draw->tv_nsec >= MIN_DRAW_INTERVAL_NS) {
            sess->ui->update(sess->ui);
            ip_get_monotonic_time(last_draw);
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

    sess->ui->error(sess->ui);
    se_clear_errors(sess);
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

static int ip_is_wed_operation(const char *key, const char **next)
{
    const char *prefix = "<wed-";
    const size_t prefix_length = strlen(prefix);
    const char *iter = key;
    size_t key_size = 0;

    while (*iter++ && key_size < prefix_length) {
        key_size++;
    }

    if (key_size < prefix_length ||
        strncmp(prefix, key, prefix_length) != 0) {
        return 0;
    }

    while (*iter && (isalpha(*iter) || *iter == '-')) {
        iter++;
    } 

    if (*iter != '>') {
        return 0;
    }

    *next = ++iter;

    return 1;
}

static void ip_get_monotonic_time(struct timespec *time)
{
#ifdef __MACH__
    clock_serv_t clock;
    mach_timespec_t mach;

    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock);
    clock_get_time(clock, &mach);
    mach_port_deallocate(mach_task_self(), clock);

    time->tv_sec = mach.tv_sec;
    time->tv_nsec = mach.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, time);
#endif
}
