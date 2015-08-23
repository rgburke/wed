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

#ifndef WED_SHARED_H
#define WED_SHARED_H

typedef enum {
    CMDT_BUFFER_MOVE = 1,
    CMDT_BUFFER_MOD  = 1 << 1,
    CMDT_CMD_INPUT   = 1 << 2,
    CMDT_EXIT        = 1 << 3,
    CMDT_SESS_MOD    = 1 << 4,
    CMDT_CMD_MOD     = 1 << 5,
    CMDT_SUSPEND     = 1 << 6
} CommandType;

typedef enum {
    WIN_MENU,
    WIN_TEXT,
    WIN_STATUS
} DrawWindow;

typedef unsigned int uint;
typedef unsigned char uchar;

#endif
