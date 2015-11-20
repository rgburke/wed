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

#ifndef WED_SEARCH_OPTIONS_H
#define WED_SEARCH_OPTIONS_H

/* Base search options common to text and regex search. These
 * values can be set and toggled by the user */
typedef struct {
    char *pattern; /* Text or regex pattern */
    size_t pattern_len; /* Pattern length */
    int case_insensitive; /* True if search should be case insensitive */
    int forward; /* True: forwards, False: backwards */
} SearchOptions;

#endif
