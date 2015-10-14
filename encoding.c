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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include "buffer.h"
#include "config.h"
#include "gap_buffer.h"

static void en_ascii_char_info(CharInfo *, CharInfoProperties, const BufferPos *, const HashMap *, uchar);
static int en_utf8_is_valid_character(const BufferPos *, size_t *);
static uint en_utf8_code_point(const uchar *, uint);

void en_utf8_char_info(CharInfo *char_info, CharInfoProperties cip, 
                      const BufferPos *pos, const HashMap *config)
{
    memset(char_info, 0, sizeof(CharInfo));
    uchar c = gb_getu_at(pos->data, pos->offset);

    if (c < 128) {
        en_ascii_char_info(char_info, cip, pos, config, c);
        return;
    }

    if (en_utf8_is_valid_character(pos, &char_info->byte_length)) {
        char_info->is_valid = 1;
    } else {
        char_info->byte_length = 1;

        while (((pos->offset + char_info->byte_length) < gb_length(pos->data)) && 
                ((uchar)gb_get_at(pos->data, pos->offset + char_info->byte_length) & 0xC0) == 0x80) {
            char_info->byte_length++;
        }
    }

    if (cip & CIP_SCREEN_LENGTH) {
        char_info->is_printable = 1;
        uchar ch[5] = { '\0' };
        gb_get_range(pos->data, pos->offset, (char *)ch, char_info->byte_length);

        if (!char_info->is_valid) {
            char_info->screen_length = 1;
        } else {
            uint code_point = en_utf8_code_point(ch, char_info->byte_length);    
            int screen_length = wcwidth(code_point);

            if (screen_length < 0) {
                char_info->screen_length = 1;
            } else {
                char_info->screen_length = screen_length;
            }
        }
    }
}

static void en_ascii_char_info(CharInfo *char_info, CharInfoProperties cip, 
                               const BufferPos *pos, const HashMap *config,
                               uchar c)
{
    assert(c < 128);

    char_info->is_valid = 1;
    char_info->byte_length = 1;

    if (cip & CIP_SCREEN_LENGTH) {
        char_info->is_printable = 1;

        if (c == '\n') {
            char_info->screen_length = 0;
        } else if (c == '\r' &&
                   *pos->file_format == FF_WINDOWS &&
                   gb_get_at(pos->data, pos->offset + 1) == '\n') {
            char_info->screen_length = 0;
        } else if (c == '\t') {
            size_t tabwidth = cf_int(config, CV_TABWIDTH);
            char_info->screen_length = tabwidth - ((pos->col_no - 1) % tabwidth);
        } else if (c < 32 || c == 127) {
            char_info->screen_length = 2;
            char_info->is_printable = 0;
        } else {
            char_info->screen_length = 1;
        }
    }
}

static int en_utf8_is_valid_character(const BufferPos *pos, size_t *char_byte_length)
{
    uchar byte = gb_get_at(pos->data, pos->offset);
    size_t byte_space_left = gb_length(pos->data) - pos->offset;
    *char_byte_length = 0;

    if (byte < 0x80) {
        *char_byte_length = 1;
    } else if (byte < 0xC2) {
        return 0;
    } else if (byte < 0xE0) {
        *char_byte_length = 2;
    } else if (byte < 0xF0) {
        *char_byte_length = 3;

        if (*char_byte_length > byte_space_left) {
            return 0;
        }

        uchar byte2 = gb_get_at(pos->data, pos->offset + 1);

        if (byte == 0xE0 && byte2 < 0xA0) {
            return 0;
        } 
    } else if (byte < 0xF5) {
        *char_byte_length = 4;

        if (*char_byte_length > byte_space_left) {
            return 0;
        }

        uchar byte2 = gb_get_at(pos->data, pos->offset + 1);

        if ((byte == 0xF0 && byte2 < 0x90) ||
            (byte == 0xF4 && byte2 >= 0x90)) {
            return 0;
        } 
    } else {
        return 0;
    }

    if (*char_byte_length > byte_space_left) {
        return 0;
    }

    for (uint k = 1; k < *char_byte_length; k++) {
        if (((uchar)gb_get_at(pos->data, pos->offset + k) & 0xC0) != 0x80) {
            return 0;
        }
    }

    return 1;
}

/* Must pass valid character */
static uint en_utf8_code_point(const uchar *character, uint byte_length)
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

/*static int utf8_is_combining_char(uint code_point)
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
}*/

size_t en_utf8_previous_char_offset(const BufferPos *pos)
{
    if (pos->offset == 0) {
        return 0;
    }

    size_t offset = pos->offset;

    do {
        offset--;
    } while (offset > 0 && ((uchar)gb_get_at(pos->data, offset) & 0xC0) == 0x80);

    return pos->offset - offset;
}
