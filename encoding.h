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

typedef unsigned int uint;
typedef unsigned char uchar;

typedef enum {
    ENC_UTF8
} CharacterEncodingType;

typedef struct {
    uint (*char_byte_length)(const char *, size_t, size_t);
    uint (*char_screen_length)(const char *, size_t, size_t, int *);
    uint (*previous_char_offset)(const char *, size_t);
} CharacterEncodingFunctions;

int init_char_enc_funcs(CharacterEncodingType, CharacterEncodingFunctions *);

#endif
