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

#ifndef WED_FILE_H
#define WED_FILE_H

#include "status.h"

typedef struct {
    char *rel_path; /* The path a user passes in as an argument */
    char *file_name; /* The file name part of rel_path */
    int exists; /* Does this file exist */
    int is_directory; /* Is this file a directory */
} FileInfo;

int init_fileinfo(FileInfo *, char *);
int init_empty_fileinfo(FileInfo *);
void free_fileinfo(FileInfo);

#endif
