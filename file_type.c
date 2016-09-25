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

#include <string.h>
#include <assert.h>
#include "file_type.h"
#include "util.h"

Status ft_init(FileType **file_type_ptr, const char *name, 
               const char *display_name, const Regex *file_pattern_regex,
               const Regex *file_content_regex)
{
    assert(file_type_ptr != NULL);
    assert(!is_null_or_empty(name));
    assert(!is_null_or_empty(display_name));
    assert(file_pattern_regex != NULL);
    assert(!is_null_or_empty(file_pattern_regex->regex_pattern));

    FileType *file_type = malloc(sizeof(FileType));

    if (file_type == NULL) {
        return OUT_OF_MEMORY("Unable to allocate filetype definition");
    }

    memset(file_type, 0, sizeof(FileType));
    Status status = STATUS_SUCCESS;

    status = ru_compile_custom_error_msg(&file_type->file_pattern,
                                         file_pattern_regex,
                                         "filetype %s ", name);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    if (file_content_regex != NULL &&
        file_content_regex->regex_pattern != NULL) {
        status = ru_compile_custom_error_msg(&file_type->file_content,
                                             file_content_regex,
                                             "filetype %s", name);

        if (!STATUS_IS_SUCCESS(status)) {
            goto cleanup;
        }
    }
    
    file_type->name = strdup(name);

    if (file_type->name == NULL) {
        status = OUT_OF_MEMORY("Unable to allocate filetype definition");
        goto cleanup;
    }

    file_type->display_name = strdup(display_name);

    if (file_type->display_name == NULL) {
        status = OUT_OF_MEMORY("Unable to allocate filetype definition");
        goto cleanup;
    }

    if (STATUS_IS_SUCCESS(status)) {
        *file_type_ptr = file_type;
    }

    return status;

cleanup:
    ft_free(file_type);

    return status;
}

void ft_free(FileType *file_type)
{
    if (file_type == NULL) {
        return;
    }

    free(file_type->name);
    free(file_type->display_name);
    ru_free_instance(&file_type->file_pattern);

    if (file_type->file_content.regex != NULL) {
        ru_free_instance(&file_type->file_content);
    }

    free(file_type);
}

Status ft_matches(FileType *file_type, FileInfo *file_info, char *file_buf,
                  size_t file_buf_size, int *matches)
{
    const char *path;
    *matches = 0;

    if (file_info->file_attrs & FATTR_EXISTS) {
        path = file_info->abs_path; 
    } else {
        path = file_info->file_name;
    }

    assert(path != NULL); 
    RegexResult result;

    Status status = ru_exec_custom_error_msg(&result, &file_type->file_pattern,
                                             path, strlen(path), 0,
                                             "filetype %s - ", file_type->name);

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    if (result.match) {
        *matches = 1;
        return STATUS_SUCCESS;
    }

    if (file_buf == NULL || file_buf_size == 0 ||
        file_type->file_content.regex == NULL) {
        return STATUS_SUCCESS;
    }

    status = ru_exec_custom_error_msg(&result, &file_type->file_content,
                                      file_buf, file_buf_size, 0,
                                      "filetype %s - ", file_type->name);

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    if (result.match) {
        *matches = 1;
    }

    return STATUS_SUCCESS;
}
