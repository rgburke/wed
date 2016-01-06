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

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "search_util.h"
#include "util.h"

EscapeSequence su_determine_escape_sequence(const char *str, size_t str_len)
{
    if (is_null_or_empty(str) || str_len < 2 || *str != '\\') {
        return ES_NONE;
    } 

    switch (str[1]) {
        case 't':
            {
                return ES_TAB;
            }
        case 'n':
            {
                return ES_NEW_LINE;
            }
        case '\\':
            {
                return ES_BACKSLASH; 
            }
        case 'x':
            {
                if (str_len > 3 &&
                    isxdigit(str[2]) &&
                    isxdigit(str[3])) {
                    return ES_HEX_NUMBER;
                }
            }
        default:
            {
                break;
            }
    }

    return ES_NONE;
}

EscapeSequenceInfo su_get_escape_sequence_info(EscapeSequence escape_sequence,
                                               int win_line_endings) {
    static EscapeSequenceInfo escape_sequences[] = {
        [ES_NONE]       = { 0, 0 },
        [ES_NEW_LINE]   = { 2, 1 },
        [ES_TAB]        = { 2, 1 },
        [ES_HEX_NUMBER] = { 4, 1 },
        [ES_BACKSLASH]  = { 2, 1 }
    }; 

    assert(escape_sequence < ARRAY_SIZE(escape_sequences, EscapeSequenceInfo));

    EscapeSequenceInfo esi = escape_sequences[escape_sequence];

    if (escape_sequence == ES_NEW_LINE && win_line_endings) {
        esi.byte_representation_length++;
    }

    return esi;
}

/* Replace supported escape sequences in find & replace text with
 * byte representations */
char *su_process_string(const char *str, size_t str_len,
                        int win_line_endings, size_t *new_str_len_ptr)
{
    if (str == NULL) {
        return NULL;
    }

    size_t new_str_len = 0;
    EscapeSequence escape_sequence;
    EscapeSequenceInfo escape_sequence_info;

    /* Determine length of str with escape sequences replaced */
    for (size_t k = 0; k < str_len; k++) {
        if (str[k] == '\\' &&
            k + 1 < str_len) {
            escape_sequence = su_determine_escape_sequence(str + k,
                                                           str_len - k);

            if (escape_sequence != ES_NONE) {
                escape_sequence_info = su_get_escape_sequence_info(
                                           escape_sequence, win_line_endings);
                k += escape_sequence_info.escape_sequence_length - 1;
                new_str_len += escape_sequence_info.byte_representation_length;
                continue;
            }
        }

        new_str_len++;
    }

    char *new_str = malloc(new_str_len + 1);

    if (new_str == NULL) {
        return NULL;
    }

    size_t new_str_index = 0;

    /* Copy str into new_str and replace escape sequences */
    for (size_t k = 0; k < str_len; k++) {
        if (str[k] == '\\' &&
            k + 1 < str_len) {

            escape_sequence = su_determine_escape_sequence(str + k,
                                                           str_len - k);
            switch (escape_sequence) {
                case ES_BACKSLASH:
                    {
                        k++;
                        break;
                    }
                case ES_TAB:
                    {
                        new_str[new_str_index++] = '\t';
                        k++;
                        continue;
                    }
                case ES_NEW_LINE:
                    {
                        if (win_line_endings) {
                            new_str[new_str_index++] = '\r';
                        } 

                        new_str[new_str_index++] = '\n';
                        k++;
                        continue;
                    }
                case ES_HEX_NUMBER:
                    {
                        if (k + 3 < str_len &&
                            isxdigit(str[k + 2]) &&
                            isxdigit(str[k + 3])) {
                            /* Generate byte reprentation of 2 digit
                             * hexadecimal number */
                            unsigned char value = 0;

                            for (size_t i = 0; i < 2; i++) {
                                int c = toupper(str[k + i + 2]);

                                if (c >= 'A') {
                                    /* Handle digits A-F:
                                     * 'A' - 55 = 10
                                     * 'B' - 55 = 11 etc...
                                     * See man 7 ascii */
                                    value += (value * 16) + (c - 55);
                                } else {
                                    /* Handle digits 0-9 */
                                    value += (value * 16) + (c - '0');
                                }
                            }

                            new_str[new_str_index++] = *(char *)&value;
                            k += 3;
                            continue;
                        }

                        break;
                    }
                case ES_NONE:
                default:
                    {
                        break;
                    }
            } 
        }

        new_str[new_str_index++] = str[k];
    }

    new_str[new_str_index] = '\0';
    *new_str_len_ptr = new_str_len;

    return new_str;
}
