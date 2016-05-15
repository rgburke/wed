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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "regex_util.h"
#include "util.h"
#include "build_config.h"

static Status re_custom_error_msg(Status, const char *fmt, va_list);

Status ru_compile(RegexInstance *reg_inst, const Regex *regex)
{
    assert(reg_inst != NULL);
    assert(regex != NULL);

    const char *error_str;
    int error_offset;

    reg_inst->regex = pcre_compile(regex->regex_pattern,
                                   PCRE_UTF8 | regex->modifiers,
                                   &error_str, &error_offset, NULL);

    if (reg_inst == NULL) {
        return st_get_error(ERR_INVALID_REGEX, "Invalid regex - %s - "
                            "at position %d", error_str, error_offset);         
    }

    reg_inst->regex_study = pcre_study(reg_inst->regex, 0, &error_str);

    return STATUS_SUCCESS;
}

static Status re_custom_error_msg(Status status, const char *fmt,
                                  va_list arg_ptr)
{
    if (STATUS_IS_SUCCESS(status)) {
        return status;
    }

    char *new_fmt = concat(fmt, status.msg);

    if (new_fmt == NULL) {
        return status;
    }

    Status new_status = st_get_custom_error(status.error_code,
                                            new_fmt, arg_ptr);

    free(new_fmt);
    st_free_status(status);

    return new_status;
}

/* Add extra info to regex error */
Status ru_compile_custom_error_msg(RegexInstance *reg_inst, const Regex *regex,
                                   const char *fmt, ...)
{
    Status status = ru_compile(reg_inst, regex);

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    status = re_custom_error_msg(status, fmt, arg_ptr);
    va_end(arg_ptr);

    return status;
}

void ru_free_instance(const RegexInstance *reg_inst)
{
    if (reg_inst == NULL) {
        return;
    }

#if WED_PCRE_VERSION_GE_8_20 && !defined(__MACH__)
    pcre_free_study(reg_inst->regex_study);
#endif
    pcre_free(reg_inst->regex);
}

Status ru_exec(RegexResult *result, const RegexInstance *reg_inst,
               const char *str, size_t str_len, size_t start)
{
    assert(str != NULL);
    assert(start < str_len);

    memset(result, 0, sizeof(RegexResult));

    result->return_code = pcre_exec(reg_inst->regex, reg_inst->regex_study,
                                    str, str_len, start, 0,
                                    result->output_vector,
                                    RE_OUTPUT_VECTOR_SIZE);

    if (result->return_code == 0) {
        return st_get_error(ERR_REGEX_EXECUTION_FAILED,
                            "Regex contains too many capture groups");
    } else if (result->return_code < 0) {
        if (result->return_code == PCRE_ERROR_NOMATCH) {
            return STATUS_SUCCESS;
        }

        return st_get_error(ERR_REGEX_EXECUTION_FAILED, 
                            "Regex execution failed. PCRE exit code: %d", 
                            result->return_code);
    }

    result->match_length = result->output_vector[1] - result->output_vector[0];
    result->match = 1;

    return STATUS_SUCCESS;
}

Status ru_exec_custom_error_msg(RegexResult *result,
                                const RegexInstance *reg_inst,
                                const char *str, size_t str_len, size_t start,
                                const char *fmt, ...)
{
    Status status = ru_exec(result, reg_inst, str, str_len, start);

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    status = re_custom_error_msg(status, fmt, arg_ptr);
    va_end(arg_ptr);

    return status;
}

/* Get captured group by number */
Status ru_get_group(const RegexResult *result, const char *str, 
                    size_t str_len, size_t group, char **group_str_ptr)
{
    assert(!is_null_or_empty(str));

    if (!result->match ||
        result->return_code <= 0 ||
        group >= (size_t)result->return_code ||
        (size_t)result->output_vector[(group * 2) + 1] > str_len) {
        return st_get_error(ERR_INVALID_REGEX_GROUP,
                            "Regex group %zu is invalid for regex result",
                            group);
    }

    size_t group_start = result->output_vector[(group * 2)];
    size_t group_end = result->output_vector[(group * 2) + 1];
    size_t group_size = group_end - group_start;

    char *group_str = malloc(group_size + 1);

    if (group_str == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to get capture group");
    }

    memcpy(group_str + group_start, str, group_size);
    *(group_str + group_end) = '\0';

    *group_str_ptr = group_str;

    return STATUS_SUCCESS;
}
