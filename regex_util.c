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
#include "regex_util.h"
#include "util.h"

static Status re_custom_error_msg(Status, const char *, va_list);

Status re_compile(RegexInstance *reg_inst, const Regex *regex)
{
    assert(reg_inst != NULL);
    assert(regex != NULL);

    const char *error_str;
    int error_offset;

    reg_inst->regex = pcre_compile(regex->regex_pattern, PCRE_UTF8 | regex->modifiers, 
                                   &error_str, &error_offset, NULL);

    if (reg_inst == NULL) {
        return st_get_error(ERR_INVALID_REGEX, "Invalid regex - %s - "
                            "at position %d", error_str, error_offset);         
    }

    reg_inst->regex_study = pcre_study(reg_inst->regex, 0, &error_str);

    return STATUS_SUCCESS;
}

static Status re_custom_error_msg(Status status, const char *fmt, va_list arg_ptr)
{
    if (STATUS_IS_SUCCESS(status)) {
        return status;
    }

    char *new_fmt = concat(fmt, status.msg);

    if (new_fmt == NULL) {
        return status;
    }

    Status new_status = st_get_custom_error(status.error_code, new_fmt, arg_ptr);

    free(new_fmt);
    st_free_status(status);

    return new_status;
}

Status re_compile_custom_error_msg(RegexInstance *reg_inst, const Regex *regex, 
                                   const char *fmt, ...)
{
    Status status = re_compile(reg_inst, regex);

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    status = re_custom_error_msg(status, fmt, arg_ptr);
    va_end(arg_ptr);

    return status;
}

void re_free_instance(const RegexInstance *reg_inst)
{
    if (reg_inst == NULL) {
        return;
    }

    pcre_free_study(reg_inst->regex_study);
    pcre_free(reg_inst->regex);
}

Status re_exec(RegexResult *result, const RegexInstance *reg_inst, 
               const char *str, size_t str_len, size_t start)
{
    assert(str != NULL);
    assert(start < str_len);

    memset(result, 0, sizeof(RegexResult));

    result->return_code = pcre_exec(reg_inst->regex, reg_inst->regex_study, str, str_len,
                                    start, 0, result->output_vector, RE_OUTPUT_VECTOR_SIZE);

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

Status re_exec_custom_error_msg(RegexResult *result, const RegexInstance *reg_inst, 
                                const char *str, size_t str_len, size_t start,
                                const char *fmt, ...)
{
    Status status = re_exec(result, reg_inst, str, str_len, start);

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    status = re_custom_error_msg(status, fmt, arg_ptr);
    va_end(arg_ptr);

    return status;
}
