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

#ifndef WED_STATUS_H
#define WED_STATUS_H

#include <stdlib.h>
#include <stdarg.h>

#define RETURN_IF_NULL(ptr) do { \
                                if ((ptr) == NULL) { \
                                    return NULL; \
                                } \
                            } while (0)

#define STATUS_ERROR(ecode, emsg, emliteral) \
                        (Status) { \
                            .error_code = (ecode), \
                            .msg = (emsg), \
                            .msg_literal = (emliteral) \
                        }

#define STATUS_SUCCESS STATUS_ERROR(ERR_NONE, NULL, 0)

#define STATUS_IS_SUCCESS(status) ((status).error_code == ERR_NONE)

#define RETURN_IF_FAIL(status) do { \
                                   Status _wed_status = (status);\
                                   if (!STATUS_IS_SUCCESS(_wed_status)) { \
                                       return _wed_status; \
                                   } \
                               } while (0)

#define MAX_ERROR_MSG_SIZE 1024
#define MAX_MSG_SIZE 1024

/* Different error types that can be reported by wed */
typedef enum {
    ERR_NONE,
    ERR_FILE_DOESNT_EXIST,
    ERR_FILE_IS_DIRECTORY,
    ERR_FILE_IS_SPECIAL,
    ERR_UNABLE_TO_OPEN_FILE,
    ERR_UNABLE_TO_READ_FILE,
    ERR_UNABLE_TO_WRITE_TO_FILE,
    ERR_INVALID_COMMAND,
    ERR_INVALID_CHARACTER,
    ERR_INVALID_STRING,
    ERR_INVALID_VAR,
    ERR_INVALID_VAL,
    ERR_INVALID_CONFIG_ENTRY,
    ERR_INCORRECT_CONFIG_LEVEL,
    ERR_INVALID_FILE_PATH,
    ERR_OUT_OF_MEMORY,
    ERR_UNABLE_TO_GET_ABS_PATH,
    ERR_INVALID_TABWIDTH,
    ERR_INVALID_FILETYPE,
    ERR_INVALID_SYNTAXTYPE,
    ERR_INVALID_CONFIG_CHARACTERS,
    ERR_INVALID_CONFIG_SYNTAX,
    ERR_FAILED_TO_PARSE_CONFIG_FILE,
    ERR_FAILED_TO_PARSE_CONFIG_INPUT,
    ERR_INVALID_BLOCK_IDENTIFIER,
    ERR_EMPTY_BLOCK_DEFINITION,
    ERR_MISSING_VARIABLE_DEFINITION,
    ERR_INVALID_STREAM,
    ERR_INVALID_ARGUMENTS,
    ERR_INVALID_BUFFERPOS,
    ERR_INVALID_REGEX,
    ERR_REGEX_EXECUTION_FAILED,
    ERR_TOO_MANY_REGEX_CAPTURE_GROUPS,
    ERR_TOO_MANY_REGEX_BACKREFERENCES,
    ERR_INVALID_CAPTURE_GROUP_BACKREFERENCE,
    ERR_INVALID_REGEX_GROUP,
    ERR_OVERRIDE_DEFAULT_THEME,
    ERR_INVALID_THEME,
    ERR_INVALID_FILE_FORMAT,
    ERR_INVALID_LINE_NO,
    ERR_NO_BUFFERS_MATCH,
    ERR_MULTIPLE_BUFFERS_MATCH,
    ERR_UNABLE_TO_OPEN_DIRECTORY,
    ERR_UNABLE_TO_READ_DIRECTORY,
    ERR_INVALID_KEY
} ErrorCode;

/* Structure used to represent success or failure */
typedef struct {
    ErrorCode error_code; /* On success error_code = ERR_NONE */
    char *msg; /* Error message */
    int msg_literal; /* True if error message is a string literal */
} Status;

Status st_get_error(ErrorCode, const char *format, ...);
Status st_get_custom_error(ErrorCode, const char *format, va_list);
void st_free_status(Status);

#endif

