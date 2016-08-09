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

#ifndef WED_UTIL_H
#define WED_UTIL_H

#include <stddef.h>
#include <stdarg.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ABS_DIFF(a, b) (MAX((a), (b)) - MIN((a), (b)))

#define ARRAY_SIZE(arr,type) (sizeof(arr) / sizeof(type))

void warn(const char *error_msg);
void fatal(const char *error_msg);
int roundup_div(size_t dividend, size_t divisor);
char *concat(const char *str1, const char *str2);
char *concat_all(size_t str_num, ...);
int is_null_or_empty(const char *str);
size_t occurrences(const char *str, const char *sub_str);
char *replace(const char *str, const char *to_replace, const char *replacement);
void *memrchr(const void *str, int val, size_t bytes);
void bytes_to_str(size_t bytes, char *buf, size_t buf_len);

#endif
