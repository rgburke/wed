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

#ifndef WED_DISPLAY_H
#define WED_DISPLAY_H

#include "session.h"
#include "buffer.h"

void init_display(const Theme *);
void resize_display(Session *);
void suspend_display(void);
void end_display(void);
void init_all_window_info(Session *);
void init_window_info(WindowInfo *);
void update_display(Session *);
void draw_errors(Session *);
size_t screen_col_no(const Buffer *, const BufferPos *);
size_t screen_height_from_screen_length(const Buffer *buffer,
                                        size_t screen_length);
void init_color_pairs(const Theme *);

#endif
