/*
 * Copyright (C) 2015 Richard Burke
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

#ifndef WED_THEME_H
#define WED_THEME_H

#include "syntax.h"

typedef enum {
    SC_LINENO,
    SC_ENTRY_NUM
} ScreenComponent;

typedef enum {
    DC_NONE,
    DC_BLACK,
    DC_RED,
    DC_GREEN,
    DC_YELLOW,
    DC_BLUE,
    DC_MAGENTA,
    DC_CYAN,
    DC_WHITE
} DrawColor;

typedef enum {
    DA_NONE,
    DA_BOLD,
    DA_UNDERLINE
} DrawAttributes;

typedef struct {
    DrawColor fg_color;
    DrawColor bg_color;
    DrawAttributes attr;
} ThemeGroup;

typedef struct {
    ThemeGroup syntax[ST_ENTRY_NUM];
    ThemeGroup screen_comp[SC_ENTRY_NUM];
} Theme;

#define TG_VAL(fgcolor,bgcolor,attrs) \
            (ThemeGroup) { \
                .fg_color = (fgcolor), \
                .bg_color = (bgcolor), \
                .attr = (attrs) \
            }

Theme *th_get_default_theme(void);
int th_str_to_draw_color(DrawColor *, const char *);
int th_str_to_screen_component(ScreenComponent *, const char *);
int th_is_valid_group_name(const char *);
void th_set_syntax_colors(Theme *, SyntaxToken, DrawColor, DrawColor);
void th_set_screen_comp_colors(Theme *, ScreenComponent, DrawColor, DrawColor);

#endif
