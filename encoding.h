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

#ifndef WED_ENCODING_H
#define WED_ENCODING_H

#include <stddef.h>
#include <assert.h>
#include "shared.h"
#include "hashmap.h"

struct BufferPos;

/* Line endings supported by wed */
/* There are currently no plans to support the old mac line endings. Use
 * mac2unix if you want to use wed */
typedef enum {
    FF_UNIX,
    FF_WINDOWS
} FileFormat;

/* Passed to en_utf8_char_info to specify the character properties we want */
typedef enum {
    CIP_DEFAULT, /* Sets is_valid,byte_length and is_printable */
    CIP_SCREEN_LENGTH /* Sets all of CIP_DEFAULT + screen_length */ 
} CharInfoProperties;

/* Structure containing properties for a single UTF-8 character at a specific
 * position */
typedef struct {
    int is_valid; /* Is character valid UTF-8 byte sequence */
    size_t byte_length; /* Number of bytes character has */
    size_t screen_length; /* The number of columns at the position specified
                             this character will take up */
    int is_printable; /* Is character printable */
} CharInfo;

void en_utf8_char_info(CharInfo *, CharInfoProperties,
                       const struct BufferPos *, const HashMap *config);
size_t en_utf8_previous_char_offset(const struct BufferPos *);

#endif
