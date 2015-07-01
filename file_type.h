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

typedef struct {
    char *name;
    char *display_name;
    pcre *file_pattern;
    pcre_extra *file_pattern_study;
} FileType;

Status ft_init(FileType **, const char *, const char *, const char *);
void ft_free(FileType *);
Status ft_matches(FileType *, FileInfo *, int *);

#endif
