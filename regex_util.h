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

/* Utility interface for regular expressions, in effect a wrapper
 * around a couple of the core pcre_* functions. For more information:
 * man pcreapi */

/* pcre_exec output vector size */
#define RE_OUTPUT_VECTOR_SIZE 90

/* Compiled PCRE regex */
typedef struct {
    pcre *regex; /* Compiled regex */
    pcre_extra *regex_study; /* Optimization info */
} RegexInstance;

/* Result of regex run */
typedef struct {
    int match; /* True if match found, equivalent to checking return_code > 0 */
    int return_code; /* pcre_exec return code */
    int output_vector[RE_OUTPUT_VECTOR_SIZE]; /* Stores captured group data */
    int match_length; /* output_vector[1] - output_vector[0] for convenience */
} RegexResult;

Status re_compile(RegexInstance *, const Regex *);
Status re_compile_custom_error_msg(RegexInstance *, const Regex *,
                                   const char *fmt, ...);
void re_free_instance(const RegexInstance *);
Status re_exec(RegexResult *, const RegexInstance *, const char *str,
               size_t str_len, size_t start);
Status re_exec_custom_error_msg(RegexResult *, const RegexInstance *,
                                const char *str, size_t str_len, size_t start,
                                const char *fmt, ...);
Status re_get_group(const RegexResult *, const char *str, size_t str_len,
                    size_t group, char **group_str_ptr);

#endif
