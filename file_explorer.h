/*
 * Copyright (C) 2016 Richard Burke
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

#ifndef WED_FILE_EXPLORER_H
#define WED_FILE_EXPLORER_H

#include "buffer.h"

/* The file explorer contains a list of files and directories present in
 * a specified directory */
typedef struct {
    char *dir_path; /* The directory which is listed */
    Buffer *buffer; /* Contains a sequence of directories and files */
    size_t dir_entries; /* The number of directory entries in the buffer */
    size_t file_entries; /* The number of file entries in the buffer */
} FileExplorer;

FileExplorer *fe_new(Buffer *buffer);
void fe_free(FileExplorer *);
Status fe_read_cwd(FileExplorer *);
Status fe_read_directory(FileExplorer *, const char *dir_path);
Buffer *fe_get_buffer(const FileExplorer *);
char *fe_get_selected(const FileExplorer *);

#endif
