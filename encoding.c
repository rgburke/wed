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

#define _XOPEN_SOURCE
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "buffer.h"

#include "unicode.c"

/* TODO make this configurable */
#define WED_TAB_SIZE 8

static int utf8_char_info(CharInfo *, CharInfoProperties, BufferPos);
static size_t utf8_previous_char_offset(const char *, size_t);
static int utf8_is_valid_character(const uchar *, size_t, size_t, size_t *);
static uint utf8_code_point(const uchar *, uint);
static int utf8_is_combining_char(uint);

static const CharacterEncodingFunctions utf8_character_encoding_functions = {
    utf8_char_info,
    utf8_previous_char_offset
};

int init_char_enc_funcs(CharacterEncodingType type, CharacterEncodingFunctions *cef)
{
    if (cef == NULL) {
        return 0;
    }

    switch (type) {
        case ENC_UTF8:
            *cef = utf8_character_encoding_functions;
            break;
        default:
            return 0;
    }

    return 1;
}

static int utf8_char_info(CharInfo *char_info, CharInfoProperties cip, BufferPos pos)
{
    if (char_info == NULL) {
        return 0;
    }

    memset(char_info, 0, sizeof(CharInfo));
    const uchar *ch = (const uchar *)(pos.line->text + pos.offset);

    if (utf8_is_valid_character(ch, pos.offset, pos.line->length, &char_info->byte_length)) {
        char_info->is_valid = 1;
    } else {
        char_info->screen_length = 1;
        return 1;
    }

    const uchar *next;
    size_t next_byte_length;

    while (pos.offset + char_info->byte_length < pos.line->length) {
        next = ch + char_info->byte_length;

        if (!utf8_is_valid_character(next, pos.offset + char_info->byte_length, pos.line->length, &next_byte_length)) {
            break;
        }
           
        if (utf8_is_combining_char(utf8_code_point(next, next_byte_length))) {
            char_info->byte_length += next_byte_length;
        } else {
            break;
        }
    }

    if (cip & CIP_SCREEN_LENGTH) {
        if (*ch == '\t') {
            char_info->screen_length = WED_TAB_SIZE - ((pos.col_no - 1) % WED_TAB_SIZE);
        } else {
            uint code_point = utf8_code_point(ch, char_info->byte_length);    
            int screen_length = wcwidth(code_point);

            if (screen_length < 0) {
                /* TODO Unprintable */
            } else {
                char_info->screen_length = screen_length;
            }
        }
    }

    return 1;
}

static int utf8_is_valid_character(const uchar *character, size_t offset, 
                                   size_t length, size_t *char_byte_length)
{
    uchar c = *character;
    size_t byte_space_left = length - offset;
    *char_byte_length = 0;

    if (c < 0x80) {
        *char_byte_length = 1;
    } else if (c < 0xC2) {
        return 0;
    } else if (c < 0xE0) {
        *char_byte_length = 2;
    } else if (c < 0xF0) {
        *char_byte_length = 3;

        if ((*char_byte_length > byte_space_left) || 
            (character[0] == 0xE0 && character[1] < 0xA0)) {
            return 0;
        } 
    } else if (c < 0xF5) {
        *char_byte_length = 4;

        if ((*char_byte_length > byte_space_left) || 
            (character[0] == 0xF0 && character[1] < 0x90) ||
            (character[0] == 0xF4 && character[1] >= 0x90)) {
            return 0;
        } 
    } else {
        return 0;
    }

    if (*char_byte_length > byte_space_left) {
        return 0;
    }

    for (uint k = 1; k < *char_byte_length; k++) {
        if ((character[k] & 0xC0) != 0x80) {
            return 0;
        }
    }

    return 1;
}

/* Must pass valid character */
static uint utf8_code_point(const uchar *character, uint byte_length)
{
   switch (byte_length) {
        case 1:
            return character[0];
        case 2:
            return ((character[0] & 0x1F) << 6) + (character[1] & 0x3F);
        case 3:
            return ((character[0] & 0x0F) << 12) + ((character[1] & 0x3F) << 6) +
                   (character[2] & 0x3F);
        case 4:
            return ((character[0] & 0x07) << 18) + ((character[1] & 0x3F) << 12) + 
                   ((character[2] & 0x3F) << 6) + (character[3] & 0x3F);
        default:
            break;
   } 

   return 0;
}

static int utf8_is_combining_char(uint code_point)
{
    if (code_point < combining[0]) {
        return 0;
    }

    int start = 0;
    int end = (sizeof(combining) / sizeof(uint)) - 1;
    int mid;

    while (start <= end) {
        mid = (start + end) / 2;  

        if (combining[mid] < code_point) {
            start = mid + 1;
        } else if (combining[mid] > code_point) {
            end = mid - 1; 
        } else {
            return 1;
        }
    }

    return 0;
}

static size_t utf8_previous_char_offset(const char *character, size_t offset)
{
    if (offset == 0) {
        return 0;
    }

    const uchar *ch = (const uchar *)character; 
    size_t start_offset = offset;

    do {
        offset--;
        ch--; 
    } while (offset > 0 && (*ch & 0xC0) == 0x80);

    return start_offset - offset;
}
