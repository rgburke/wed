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

struct BufferPos;
typedef unsigned int uint;
typedef unsigned char uchar;

typedef enum {
    ENC_UTF8
} CharacterEncodingType;

typedef enum {
    CIP_DEFAULT,
    CIP_SCREEN_LENGTH   
} CharInfoProperties;

typedef struct {
    int is_valid;
    size_t byte_length;
    size_t screen_length;
} CharInfo;

typedef struct {
    int (*char_info)(CharInfo *, CharInfoProperties, struct BufferPos);
    size_t (*previous_char_offset)(const char *, size_t);
} CharacterEncodingFunctions;

int init_char_enc_funcs(CharacterEncodingType, CharacterEncodingFunctions *);

#endif