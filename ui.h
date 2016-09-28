/*
 * Copyright (C) 2016 Richard Burke
 * Inspired by ui.h from the vis text editor by Marc André Tanner
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

#ifndef WED_UI_H
#define WED_UI_H

#include "status.h"

/* UI interface. Currently implemented by TUI in tui.h, but could be
 * implemented by other UI types (e.g. X11) if desired */

typedef struct UI UI;

struct UI {
    Status (*init)(UI *); /* Initialise UI */
    Status (*get_input)(UI *); /* Get user input */
    Status (*update)(UI *); /* Update display */
    Status (*error)(UI *); /* Display an error */
    Status (*update_theme)(UI *); /* User has changed the active theme so
                                     update theme related data */
    Status (*resize)(UI *); /* The display window has been resized */
    Status (*suspend)(UI *); /* The display is about to be suspended */
    Status (*resume)(UI *); /* Resume the display */
    Status (*end)(UI *); /* End UI */
    Status (*free)(UI *); /* Free any allocated memory */
};

#endif
