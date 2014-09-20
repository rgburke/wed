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

void edit(Session *sess)
{
    int c, quit = 0;

    while (!quit) {
        c = getch();
        add_error(sess, do_command(sess, c, &quit));    
        update_display(sess);
    }
}
