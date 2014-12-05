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

#include <ncurses.h>
#include "input.h"
#include "session.h"
#include "display.h"
#include "buffer.h"
#include "command.h"
#include "status.h"
#include "lib/libtermkey/termkey.h"

#define MAX_KEY_STR_SIZE 50

static TermKey *termkey = NULL;

void edit(Session *sess)
{
    termkey = termkey_new(0, TERMKEY_FLAG_SPACESYMBOL | TERMKEY_FLAG_CTRLC);

    if (termkey == NULL) {
        /* TODO Need to add out of memory type error.
         * Also need to deal with fatal out of memory cases like this. */
        return;
    }

    init_display();
    init_all_window_info(sess);
    refresh_display(sess);

    process_input(sess);

    end_display();
    termkey_destroy(termkey);
}

void process_input(Session *sess)
{
    if (termkey == NULL) {
        return;
    }

    char keystr[MAX_KEY_STR_SIZE];
    TermKeyResult ret;
    TermKeyKey key;
    int finished = 0;

    while (!finished) {
        ret = termkey_waitkey(termkey, &key);

        if (ret == TERMKEY_RES_KEY) {
            termkey_strfkey(termkey, keystr, sizeof(keystr), &key, TERMKEY_FORMAT_VIM);
            add_error_if_fail(sess, do_command(sess, keystr, &finished));
            update_display(sess);
        }
    }
}
