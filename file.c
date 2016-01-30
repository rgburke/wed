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

#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <assert.h>
#include "file.h"
#include "util.h"
#include "status.h"

Status fi_init(FileInfo *file_info, const char *path)
{
    assert(!is_null_or_empty(path));
    memset(file_info, 0, sizeof(FileInfo));

    file_info->rel_path = fi_process_path(path);

    if (file_info->rel_path == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, 
                            "Out of memory - Unable to determine"
                            " fileinfo for file %s", 
                            path);
    }

    file_info->file_name = basename(file_info->rel_path);
    file_info->file_attrs = FATTR_NONE;

    int exists = (stat(file_info->rel_path, &file_info->file_stat) == 0);

    if (!exists) {
        return STATUS_SUCCESS;
    }

    file_info->file_attrs |= FATTR_EXISTS;

    if (S_ISDIR(file_info->file_stat.st_mode)) {
        file_info->file_attrs |= FATTR_DIR;
        return STATUS_SUCCESS;
    } else if (!S_ISREG(file_info->file_stat.st_mode)) {
        file_info->file_attrs |= FATTR_SPECIAL;
        return STATUS_SUCCESS;
    }

    file_info->abs_path = malloc(PATH_MAX + 1);

    if (file_info->abs_path == NULL) {
        free(file_info->rel_path);
        return st_get_error(ERR_OUT_OF_MEMORY, 
                            "Out of memory - Unable to determine"
                            " fileinfo for file %s", 
                            path);
    }

    /* Get the canonicalized absolute pathname.
     * See man 3 realpath */
    if (realpath(file_info->rel_path, file_info->abs_path) == NULL) {
        free(file_info->rel_path);
        free(file_info->abs_path);
        return st_get_error(ERR_UNABLE_TO_GET_ABS_PATH,
                            "Unable to determine absolute path"
                            " for file %s - %s", path,
                            strerror(errno));
    }
     
    fi_check_can_read_file(file_info);
    fi_check_can_write_file(file_info);

    return STATUS_SUCCESS;
}

/* For when a buffer represents a file which doesn't exist yet.
 * i.e. open wed with no arguments: [new 1] */
int fi_init_empty(FileInfo *file_info, const char *file_name)
{
    assert(!is_null_or_empty(file_name));

    memset(file_info, 0, sizeof(FileInfo));

    /* Leave rel_path NULL and just set the file name part
     * so we have something to display. wed will detect later
     * that no path is specified and prompt the user to 
     * enter one if necessary */
    file_info->file_name = strdup(file_name);

    if (file_info->file_name == NULL) {
        return 0;
    }

    return 1;
}

Status fi_init_stdin(FileInfo *file_info, const char *path)
{
    RETURN_IF_FAIL(fi_init(file_info, path));
    
    if (file_info->abs_path == NULL) {
        file_info->abs_path = strdup(path);

        if (file_info->abs_path == NULL) {
            return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - "
                                "Unable copy file path");
        }
    }

    fi_check_can_read_file(file_info);
    fi_check_can_write_file(file_info);

    return STATUS_SUCCESS;
}

void fi_free(FileInfo *file_info)
{
    if (fi_file_exists(file_info)) {
        free(file_info->rel_path);
        free(file_info->abs_path);
    } else if (fi_has_file_path(file_info)) {
        free(file_info->rel_path);
    } else {
        free(file_info->file_name);
    }
}

/* Replace ~ at the start of a file path with
 * $HOME in the same way a shell does */
char *fi_process_path(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    int home_dir_path = (path[0] == '~');

    if (home_dir_path) {
        return concat(getenv("HOME"), path + 1);
    }

    return strdup(path);
}

int fi_is_directory(const FileInfo *file_info)
{
    return file_info->file_attrs & FATTR_DIR;
}

int fi_is_special(const FileInfo *file_info)
{
    return file_info->file_attrs & FATTR_SPECIAL;
}

int fi_file_exists(const FileInfo *file_info)
{
    return file_info->file_attrs & FATTR_EXISTS;
}

int fi_has_file_path(const FileInfo *file_info)
{
    return file_info->rel_path != NULL;
}

int fi_check_file_exists(FileInfo *file_info)
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

int fi_can_read_file(const FileInfo *file_info)
{
    return file_info->file_attrs & FATTR_READABLE;
}

int fi_check_can_read_file(FileInfo *file_info)
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

int fi_can_write_file(const FileInfo *file_info)
{
    return file_info->file_attrs & FATTR_WRITABLE;
}

int fi_check_can_write_file(FileInfo *file_info)
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

int fi_refresh_file_attributes(FileInfo *file_info)
{
    return (fi_check_file_exists(file_info) &
            fi_check_can_read_file(file_info) &
            fi_check_can_write_file(file_info) &
            (stat(file_info->abs_path, &file_info->file_stat) != 0));
}

int fi_equal(const FileInfo *f1, const FileInfo *f2)
{
    int f1_exists = f1->file_attrs & FATTR_EXISTS;
    int f2_exists = f2->file_attrs & FATTR_EXISTS;
    
    if (!f1_exists && !f2_exists) {
        if (f1->rel_path == NULL || f2->rel_path == NULL) {
            return 0;
        }
        /* TODO As the paths are not canonical 
         * this is not a true test of path equality. */
        return strcmp(f1->rel_path, f2->rel_path) == 0;
    } else if (!(f1_exists && f2_exists)) {
        return 0;
    }

    return f1->file_stat.st_dev == f2->file_stat.st_dev &&
           f1->file_stat.st_ino == f2->file_stat.st_ino;
}

