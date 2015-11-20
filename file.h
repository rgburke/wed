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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "status.h"

/* File attributes that are checked for */
typedef enum {
    FATTR_NONE = 0,
    FATTR_EXISTS = 1 << 0,
    FATTR_DIR = 1 << 1,
    FATTR_SPECIAL = 1 << 2,
    FATTR_READABLE = 1 << 3,
    FATTR_WRITABLE = 1 << 4
} FileAttributes; 

/* File data struct. Only certain fields are set depending on whether
 * an underlying file on disk exists */
typedef struct {
    char *rel_path; /* The path entered by the user */
    char *file_name; /* The file name part of rel_path
                        i.e. points to part of rel_path */
    char *abs_path; /* The absolute path (only if file exists) */
    struct stat file_stat; /* See man 2 stat */
    FileAttributes file_attrs; /* Bit mask for file attributes */
} FileInfo;

Status fi_init(FileInfo *, const char *path);
int fi_init_empty(FileInfo *, const char *file_name);
void fi_free(FileInfo *);
char *fi_process_path(const char *path);
int fi_is_directory(const FileInfo *);
int fi_is_special(const FileInfo *);
int fi_file_exists(const FileInfo *);
int fi_has_file_path(const FileInfo *);
int fi_check_file_exists(FileInfo *);
int fi_can_read_file(const FileInfo *);
int fi_check_can_read_file(FileInfo *);
int fi_can_write_file(const FileInfo *);
int fi_check_can_write_file(FileInfo *);
int fi_refresh_file_attributes(FileInfo *);
int fi_equal(const FileInfo *, const FileInfo *);

#endif
