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

#define FT_OUTPUT_VECTOR_SIZE 30

Status ft_init(FileType **file_type_ptr, const char *name, const char *display_name, const char *file_pattern)
{
    assert(file_type_ptr != NULL);
    assert(!is_null_or_empty(name));
    assert(!is_null_or_empty(display_name));
    assert(!is_null_or_empty(file_pattern));

    FileType *file_type = malloc(sizeof(FileType));

    if (file_type == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - " 
                            "Unable to allocate filetype definition");
    }

    memset(file_type, 0, sizeof(FileType));

    Status status = STATUS_SUCCESS;
    const char *error_str;
    int error_offset;

    file_type->file_pattern = pcre_compile(file_pattern, PCRE_UTF8, &error_str, &error_offset, NULL);

    if (file_type->file_pattern == NULL) {
        status =  st_get_error(ERR_INVALID_REGEX, "filetype %s invalid regex - "
                              "%s - at position %d", name, error_str, error_offset);         
        goto cleanup;
    }

    file_type->file_pattern_study = pcre_study(file_type->file_pattern, 0, &error_str);
    
    file_type->name = strdupe(name);

    if (file_type->name == NULL) {
        status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - " 
                              "Unable to allocate filetype definition");
        goto cleanup;
    }

    file_type->display_name = strdupe(display_name);

    if (file_type->display_name == NULL) {
        status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - " 
                              "Unable to allocate filetype definition");
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
    pcre_free_study(file_type->file_pattern_study);
    pcre_free(file_type->file_pattern);
    free(file_type);
}

Status ft_matches(FileType *file_type, FileInfo *file_info, int *matches)
{
    const char *path;
    *matches = 0;

    if (file_info->file_attrs & FATTR_EXISTS) {
        path = file_info->abs_path; 
    } else {
        path = file_info->file_name;
    }

    assert(path != NULL); 

    int output_vector[FT_OUTPUT_VECTOR_SIZE];

    int return_code = pcre_exec(file_type->file_pattern, file_type->file_pattern_study, 
                                path, strlen(path), 0, 0, output_vector, FT_OUTPUT_VECTOR_SIZE);

    if (return_code < 0) {
        if (return_code != PCRE_ERROR_NOMATCH) {
            return st_get_error(ERR_REGEX_EXECUTION_FAILED, 
                                "Regex execution failed. PCRE exit code: %d", 
                                return_code);
        }
    } else {
        *matches = 1;
    }

    return STATUS_SUCCESS;
}
