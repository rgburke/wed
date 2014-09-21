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

void edit(Session *sess)
{
    int quit = 0;
    char keystr[MAX_KEY_STR_SIZE];
    TermKey *termkey = termkey_new(0, TERMKEY_FLAG_SPACESYMBOL | TERMKEY_FLAG_CTRLC);
    TermKeyResult ret;
    TermKeyKey key;

    if (termkey == NULL) {
        /* TODO Need to add out of memory type error.
         * Also need to deal with fatal out of memory cases like this. */
        return;
    }

    init_display();
    refresh_display(sess);

    while (!quit) {
        ret = termkey_waitkey(termkey, &key);

        if (ret == TERMKEY_RES_KEY) {
            termkey_strfkey(termkey, keystr, sizeof(keystr), &key, TERMKEY_FORMAT_VIM);
            add_error(sess, do_command(sess, keystr, &quit));    
        }

        update_display(sess);
    }

    end_display();
    termkey_destroy(termkey);
}
