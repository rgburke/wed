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

/* This enum unifies tokens with items that appear
 * on the screen, i.e. it unifies all drawn items */
typedef enum {
    SC_LINENO = ST_ENTRY_NUM,
    SC_BUFFER_TAB_BAR,
    SC_ACTIVE_BUFFER_TAB_BAR,
    SC_STATUS_BAR,
    SC_ERROR_MESSAGE,
    SC_BUFFER_END,
    SC_COLORCOLUMN,
    SC_ENTRY_NUM
} ScreenComponent;

/* The colors available in wed. These map directly to
 * the standard colors in ncurses */
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

/* Attributes that can be applied to drawn text */
/* TODO Currently ignored. Also shouldn't this be a bitmask? */
typedef enum {
    DA_NONE,
    DA_BOLD,
    DA_UNDERLINE
} DrawAttributes;

/* All modifiable draw properties */
typedef struct {
    DrawColor fg_color; /* Foreground color */
    DrawColor bg_color; /* Background color */
    DrawAttributes attr; /* Attributes that can be
                            applied to drawn text */
} ThemeGroup;

/* Structure that stores a theme. This struct
 * maps screen components to theme groups.
 * This allows all drawable components to have
 * custom draw properties set for them.
 * This in turn allows the user to specify custom colouring
 * for screen components using theme config definitions
 * i.e. specify their own themes */
typedef struct {
    ThemeGroup groups[SC_ENTRY_NUM];
} Theme;

#define TG_VAL(fgcolor,bgcolor,attrs) \
            (ThemeGroup) { \
                .fg_color = (fgcolor), \
                .bg_color = (bgcolor), \
                .attr = (attrs) \
            }

Theme *th_get_default_theme(void);
int th_str_to_draw_color(DrawColor *, const char *draw_color_str);
int th_str_to_screen_component(ScreenComponent *, const char *screen_comp_str);
int th_is_valid_group_name(const char *group_name);
void th_set_screen_comp_colors(Theme *, uint screen_comp,
                               DrawColor fg_color, DrawColor bg_color);
ThemeGroup th_get_theme_group(const Theme *, uint screen_comp);

#endif
