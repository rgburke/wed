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

#ifndef WED_FILE_TYPE_H
#define WED_FILE_TYPE_H

#include <pcre.h>
#include "status.h"
#include "file.h"
#include "regex_util.h"

/* This is a high level classification which can in turn
 * be used to drive other features such as syntax highlighting 
 * e.g. If a buffer is of the c FileType then the c syntax definition
 * can be loaded and applied for that buffer. */
typedef struct {
    char *name; /* The id of the FileType. Other constructs such as syntax
                   definitions use the same name as a relevant file type,
                   so that they can be loaded by a file type name */
    char *display_name; /* A more human readable name that can be used for
                           display */
    RegexInstance file_pattern; /* A regex applied to a file path in order
                                   to determine membership of a file type */
    RegexInstance file_content; /* A regex applied to the first line of
                                   of a file */
} FileType;

Status ft_init(FileType **file_type_ptr, const char *name, 
               const char *display_name, const Regex *file_pattern_regex,
               const Regex *file_content_regex);
void ft_free(FileType *);
Status ft_matches(FileType *, FileInfo *, char *file_buf,
                  size_t file_buf_size, int *matches);

#endif
