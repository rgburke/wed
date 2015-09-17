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

#ifndef WED_REGEX_UTIL_H
#define WED_REGEX_UTIL_H

#include <stdarg.h>
#include <pcre.h>
#include "status.h"
#include "value.h"

#define RE_OUTPUT_VECTOR_SIZE 90

typedef struct {
    pcre *regex;
    pcre_extra *regex_study;
} RegexInstance;

typedef struct {
    int match;
    int return_code;
    int output_vector[RE_OUTPUT_VECTOR_SIZE];
    /* output_vector[1] - output_vector[0] for convenience */
    int match_length;
} RegexResult;

Status re_compile(RegexInstance *, const Regex *);
Status re_compile_custom_error_msg(RegexInstance *, const Regex *, const char *, ...);
void re_free_instance(const RegexInstance *);
Status re_exec(RegexResult *, const RegexInstance *, const char *, size_t, size_t);
Status re_exec_custom_error_msg(RegexResult *, const RegexInstance *, const char *, 
                                size_t, size_t, const char *, ...);
Status re_get_group(const RegexResult *, const char *, size_t, size_t, char **);

#endif
