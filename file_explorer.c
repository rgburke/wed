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

/* For DT_DIR, DT_REG, DT_DIR and DT_LNK */
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include "file_explorer.h"
#include "util.h"

/* The maximum number of directory entries that are stored */
#define MAX_DIR_ENT_NUM 1000
/* The maximum size of a directory entry name as given by: man 3 readdir */
#define MAX_DNAME_SIZE 256

/* Classify each directory entry */
typedef enum {
    DET_FILE,
    DET_DIRECTORY
} DirectoryEntryType;

/* Represents a directory entry. This structure is used for temporary storage
 * before entries are added to the buffer */
typedef struct {
    char name[MAX_DNAME_SIZE];
    DirectoryEntryType type;
} DirectoryEntry;

static int fe_cmp_de(const void *, const void *);

FileExplorer *fe_new(Buffer *buffer)
{
    FileExplorer *file_explorer = malloc(sizeof(FileExplorer));
    RETURN_IF_NULL(file_explorer);
    memset(file_explorer, 0, sizeof(FileExplorer));

    file_explorer->buffer = buffer;

    return file_explorer;
}

void fe_free(FileExplorer *file_explorer)
{
    if (file_explorer == NULL) {
        return;
    }

    bf_free(file_explorer->buffer);
    free(file_explorer->dir_path);
    free(file_explorer);
}

Status fe_read_cwd(FileExplorer *file_explorer)
{
    char cwd[PATH_MAX];
    char *cwd_ptr = getcwd(cwd, sizeof(cwd));

    if (cwd_ptr == NULL) {
        return st_get_error(ERR_UNABLE_TO_DETERMINE_CWD,
                            "Unable to determine current working directory: %s",
                            strerror(errno));
    }

    return fe_read_directory(file_explorer, cwd);
}

Status fe_read_directory(FileExplorer *file_explorer, const char *dir_path)
{
    static DirectoryEntry entries[MAX_DIR_ENT_NUM];
    Status status = STATUS_SUCCESS;

    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        return st_get_error(ERR_UNABLE_TO_OPEN_DIRECTORY,
                            "Unable to open directory %s for reading: %s",
                            dir_path, strerror(errno));
    }

    Buffer *buffer = file_explorer->buffer;
    RETURN_IF_FAIL(bf_reset(buffer));

    file_explorer->dir_path = strdup(dir_path);

    if (file_explorer->dir_path == NULL) {
        return OUT_OF_MEMORY("Unable to allocate directory path");
    }

    file_explorer->dir_entries = 0;
    file_explorer->file_entries = 0;

    struct dirent *entry;
    size_t entry_index = 0;
    DirectoryEntry *de;
    errno = 0;

    while (entry_index < MAX_DIR_ENT_NUM && (entry = readdir(dir)) != NULL) {
        if (strcmp(".", entry->d_name) == 0 ||
            strcmp("..", entry->d_name) == 0 ||
            (entry->d_type != DT_REG &&
             entry->d_type != DT_DIR &&
             entry->d_type != DT_LNK)) {
            continue;
        }

        de = &entries[entry_index++];

        snprintf(de->name, MAX_DNAME_SIZE, "%s", entry->d_name); 
        
        if (entry->d_type == DT_DIR) {
            de->type = DET_DIRECTORY;
            file_explorer->dir_entries++;
        } else {
            de->type = DET_FILE;
            file_explorer->file_entries++;
        } 
    }

    if (errno) {
        status = st_get_error(ERR_UNABLE_TO_READ_DIRECTORY,
                              "Unable to read from directory %s: %s",
                              dir_path, strerror(errno));
    }

    closedir(dir);
    RETURN_IF_FAIL(status);

    const size_t entry_num = entry_index;
    qsort(entries, entry_num, sizeof(DirectoryEntry), fe_cmp_de);

    if (strcmp("/", dir_path) != 0) {
        RETURN_IF_FAIL(bf_insert_string(buffer, "../", 3, 1));

        if (entry_num > 0) {
            RETURN_IF_FAIL(bf_insert_string(buffer, "\n", 1, 1));
        }

        file_explorer->dir_entries++;
    }

    for (entry_index = 0; entry_index < entry_num; entry_index++) {
        de = &entries[entry_index];

        RETURN_IF_FAIL(bf_insert_string(buffer, de->name, strlen(de->name), 1)); 

        if (de->type == DET_DIRECTORY) {
            RETURN_IF_FAIL(bf_insert_string(buffer, "/", 1, 1));
        }

        if (entry_index < entry_num - 1) {
            RETURN_IF_FAIL(bf_insert_string(buffer, "\n", 1, 1));
        }
    }

    RETURN_IF_FAIL(bf_to_buffer_start(buffer, 0));

    return status;
}

static int fe_cmp_de(const void *o1, const void *o2)
{
    const DirectoryEntry *de1 = (const DirectoryEntry *)o1;
    const DirectoryEntry *de2 = (const DirectoryEntry *)o2;

    if (de1->type == de2->type) {
        return strncasecmp(de1->name, de2->name, MAX_DNAME_SIZE);
    }

    if (de1->type == DET_DIRECTORY) {
        return -1;
    } else if (de2->type == DET_DIRECTORY) {
        return 1;
    }

    return 0;
}

Buffer *fe_get_buffer(const FileExplorer *file_explorer)
{
    return file_explorer->buffer;
}

char *fe_get_selected(const FileExplorer *file_explorer)
{
    if (file_explorer->dir_path == NULL ||
        (file_explorer->dir_entries == 0 &&
         file_explorer->file_entries == 0)) {
        return NULL;
    }

    static char entry_name[MAX_DNAME_SIZE];
    const Buffer *buffer = file_explorer->buffer;
    const BufferPos *pos = &buffer->pos;
    size_t entry_name_len = bf_get_line(buffer, pos, entry_name,
                                        MAX_DNAME_SIZE);

    if (entry_name_len == MAX_DNAME_SIZE) {
        entry_name_len--;
    }

    entry_name[entry_name_len] = '\0';
    const char *dir_path = file_explorer->dir_path;
    const size_t dir_path_len = strlen(dir_path);
    const char *path_separator = "/";
    
    if (dir_path[dir_path_len - 1] == '/') {
        path_separator = "";
    }

    return concat_all(3, dir_path, path_separator, entry_name);
}

