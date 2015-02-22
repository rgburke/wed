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

#include "wed.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "status.h"

typedef enum {
    FATTR_NONE = 0,
    FATTR_EXISTS = 1 << 0,
    FATTR_DIR = 1 << 1,
    FATTR_SPECIAL = 1 << 2,
    FATTR_READABLE = 1 << 3,
    FATTR_WRITABLE = 1 << 4
} FileAttributes; 

typedef struct {
    char *rel_path; /* The path entered by the user */
    char *file_name; /* The file name part of rel_path */
    char *abs_path; /* The absolute path (only if file exists) */
    struct stat file_stat;
    FileAttributes file_attrs;
} FileInfo;

Status init_fileinfo(FileInfo *, const char *);
int init_empty_fileinfo(FileInfo *);
void free_fileinfo(FileInfo);
int file_is_directory(FileInfo);
int file_is_special(FileInfo);
int file_exists(FileInfo);
int check_file_exists(FileInfo *);
int can_read_file(FileInfo);
int check_can_read_file(FileInfo *);
int can_write_file(FileInfo);
int check_can_write_file(FileInfo *);
int refresh_file_attributes(FileInfo *);
int file_info_equal(FileInfo, FileInfo);

#endif
