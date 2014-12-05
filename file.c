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

#include "wed.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include "file.h"
#include "util.h"

int init_fileinfo(FileInfo *file_info, char *path)
{
    if (file_info == NULL || path == NULL) {
        return 0;
    }

    file_info->rel_path = strdupe(path);
    file_info->file_name = basename(file_info->rel_path);
    file_info->file_attrs = FATTR_NONE;

    struct stat file_stat;

    if (stat(path, &file_stat) == 0) {
        file_info->file_attrs |= FATTR_EXISTS;
    }

    if (file_info->file_attrs & FATTR_EXISTS) {
        if (S_ISDIR(file_stat.st_mode)) {
            file_info->file_attrs |= FATTR_DIR;
            return 1;
        }

        check_can_read_file(file_info);
        check_can_write_file(file_info);
    } 

    return 1;
}

/* For when a buffer represented a file which doesn't exist yet. */
int init_empty_fileinfo(FileInfo *file_info)
{
    if (file_info == NULL) {
        return 0;
    }

    file_info->rel_path = NULL;
    file_info->file_name = "No Name";
    file_info->file_attrs = FATTR_NONE;

    return 1;
}

void free_fileinfo(FileInfo file_info)
{
    free(file_info.rel_path);
}

int file_is_directory(FileInfo file_info)
{
    return file_info.file_attrs & FATTR_DIR;
}

int file_exists(FileInfo file_info)
{
    return file_info.file_attrs & FATTR_EXISTS;
}

int check_file_exists(FileInfo *file_info)
{
    if (file_info->rel_path == NULL) {
        return 0;
    }

    int exists = access(file_info->rel_path, F_OK) == 0;

    if (exists) {
        file_info->file_attrs |= FATTR_EXISTS;
    } else {
        file_info->file_attrs = FATTR_NONE;
    }

    return exists;
}

int can_read_file(FileInfo file_info)
{
    return file_info.file_attrs & FATTR_READABLE;
}

int check_can_read_file(FileInfo *file_info)
{
    if (file_info->rel_path == NULL) {
        return 0;
    }

    int can_read = access(file_info->rel_path, R_OK) == 0;

    if (can_read) {
        file_info->file_attrs |= FATTR_READABLE;
    } else {
        file_info->file_attrs &= ~FATTR_READABLE;
    }

    return can_read;
}

int can_write_file(FileInfo file_info)
{
    return file_info.file_attrs & FATTR_WRITABLE;
}

int check_can_write_file(FileInfo *file_info)
{
    if (file_info->rel_path == NULL) {
        return 0;
    }

    int can_write = access(file_info->rel_path, W_OK) == 0;

    if (can_write) {
        file_info->file_attrs |= FATTR_WRITABLE;
    } else {
        file_info->file_attrs &= ~FATTR_WRITABLE;
    }

    return can_write;
}

int set_file_path(FileInfo *file_info, const char *file_path)
{
    if (file_info == NULL || file_path == NULL) {
        return 0;
    }

    if (file_info->rel_path != NULL) {
        free(file_info->rel_path);
    }

    file_info->rel_path = strdupe(file_path);
    file_info->file_name = basename(file_info->rel_path);

    return 1;
}

