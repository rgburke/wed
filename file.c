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

int init_fileinfo(FileInfo *file_info, char *path)
{
    if (file_info == NULL || path == NULL) {
        return 0;
    }

    file_info->rel_path = strdup(path);
    file_info->file_name = basename(file_info->rel_path);
    struct stat file_stat;

    file_info->exists = (stat(path, &file_stat) == 0);

    if (file_info->exists) {
        file_info->is_directory = S_ISDIR(file_stat.st_mode);
    } else {
        file_info->is_directory = 0;
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
    file_info->exists = 0;
    file_info->is_directory = 0;

    return 1;
}

void free_fileinfo(FileInfo file_info)
{
    free(file_info.rel_path);
}
