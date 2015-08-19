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

#include <string.h>
#include <assert.h>
#include "theme.h"

Theme *th_get_default_theme(void)
{
    Theme *theme = malloc(sizeof(Theme));
    RETURN_IF_NULL(theme);

    theme->groups[ST_NORMAL]     = TG_VAL(DC_NONE   , DC_NONE  , DA_NONE);
    theme->groups[ST_COMMENT]    = TG_VAL(DC_BLUE   , DC_NONE  , DA_NONE);
    theme->groups[ST_CONSTANT]   = TG_VAL(DC_RED    , DC_NONE  , DA_NONE);
    theme->groups[ST_SPECIAL]    = TG_VAL(DC_MAGENTA, DC_NONE  , DA_NONE);
    theme->groups[ST_IDENTIFIER] = TG_VAL(DC_CYAN   , DC_NONE  , DA_NONE);
    theme->groups[ST_STATEMENT]  = TG_VAL(DC_YELLOW , DC_NONE  , DA_NONE);
    theme->groups[ST_TYPE]       = TG_VAL(DC_GREEN  , DC_NONE  , DA_NONE);
    theme->groups[ST_ERROR]      = TG_VAL(DC_WHITE  , DC_RED   , DA_NONE);
    theme->groups[ST_TODO]       = TG_VAL(DC_NONE   , DC_YELLOW, DA_NONE);

    theme->groups[SC_LINENO]                = TG_VAL(DC_YELLOW, DC_NONE , DA_NONE);
    theme->groups[SC_BUFFER_TAB_BAR]        = TG_VAL(DC_BLUE  , DC_WHITE, DA_NONE);
    theme->groups[SC_ACTIVE_BUFFER_TAB_BAR] = TG_VAL(DC_BLUE  , DC_NONE , DA_NONE);
    theme->groups[SC_STATUS_BAR]            = TG_VAL(DC_YELLOW, DC_BLUE , DA_NONE);
    theme->groups[SC_ERROR_MESSAGE]         = TG_VAL(DC_WHITE , DC_RED  , DA_NONE);
    theme->groups[SC_BUFFER_END]            = TG_VAL(DC_BLUE  , DC_NONE , DA_NONE);

    return theme;
}

int th_str_to_draw_color(DrawColor *draw_color_ptr, const char *draw_color_str)
{
    assert(draw_color_str != NULL);

    static const char *draw_colors[] = {
        [DC_NONE]    = "none",
        [DC_BLACK]   = "black",
        [DC_RED]     = "red",
        [DC_GREEN]   = "green",
        [DC_YELLOW]  = "yellow",
        [DC_BLUE]    = "blue",
        [DC_MAGENTA] = "magenta",
        [DC_CYAN]    = "cyan",
        [DC_WHITE]   = "white"
    };

    static const size_t draw_color_num = sizeof(draw_colors) / sizeof(const char *);

    for (size_t k = 0; k < draw_color_num; k++) {
        if (strcmp(draw_colors[k], draw_color_str) == 0) {
            *draw_color_ptr = k;
            return 1;
        }
    }

    return 0;
}

int th_str_to_screen_component(ScreenComponent *screen_comp_ptr, 
                               const char *screen_comp_str)
{
    assert(screen_comp_str != NULL);

    static const char *screen_comps[] = {
        [SC_LINENO]                = "lineno",
        [SC_BUFFER_TAB_BAR]        = "buffertabbar",
        [SC_ACTIVE_BUFFER_TAB_BAR] = "activebuffertabbar",
        [SC_STATUS_BAR]            = "statusbar",
        [SC_ERROR_MESSAGE]         = "errormessage",
        [SC_BUFFER_END]            = "bufferend"
    };

    for (size_t k = ST_ENTRY_NUM; k < SC_ENTRY_NUM; k++) {
        if (strcmp(screen_comps[k], screen_comp_str) == 0) {
            *screen_comp_ptr = k;
            return 1;
        }
    }

    return 0;
}

int th_is_valid_group_name(const char *group_name)
{
    assert(group_name != NULL);

    SyntaxToken token;
    ScreenComponent screen_comp;

    return sy_str_to_token(&token, group_name) ||
           th_str_to_screen_component(&screen_comp, group_name);
}

void th_set_screen_comp_colors(Theme *theme, uint screen_comp,
                               DrawColor fg_color, DrawColor bg_color)
{
    assert(screen_comp < SC_ENTRY_NUM);

    theme->groups[screen_comp] = TG_VAL(fg_color, bg_color, DA_NONE);
}

ThemeGroup th_get_theme_group(const Theme *theme, uint screen_comp)
{
    assert(screen_comp < SC_ENTRY_NUM);

    return theme->groups[screen_comp];
}
