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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "status.h"
#include "value.h"
#include "util.h"

static char *st_get_default_error_message(ErrorCode);

typedef struct {
    ErrorCode error_code;
    char *error_msg;
} ErrorCodeMsg;

static const ErrorCodeMsg default_error_messages[] = {
    { ERR_FILE_DOESNT_EXIST                  , "File doesn't exist"                  },
    { ERR_FILE_IS_DIRECTORY                  , "File is a directory"                 },
    { ERR_FILE_IS_SPECIAL                    , "File is not regular"                 },
    { ERR_UNABLE_TO_OPEN_FILE                , "Unable to open file"                 },
    { ERR_UNABLE_TO_READ_FILE                , "Unable to read from file"            },
    { ERR_UNABLE_TO_WRITE_TO_FILE            , "Unable to write to file"             },
    { ERR_INVALID_COMMAND                    , "Invalid command"                     },
    { ERR_INVALID_CHARACTER                  , "Invalid character"                   },
    { ERR_INVALID_STRING                     , "Invalid string"                      },
    { ERR_INVALID_VAR                        , "Invalid variable"                    },
    { ERR_INVALID_VAL                        , "Invalid value"                       },
    { ERR_INVALID_CONFIG_ENTRY               , "Invalid config entry"                },
    { ERR_INCORRECT_CONFIG_LEVEL             , "Incorrect config level"              },
    { ERR_INVALID_FILE_PATH                  , "Invalid file path"                   },
    { ERR_OUT_OF_MEMORY                      , "Out of memory"                       },
    { ERR_UNABLE_TO_GET_ABS_PATH             , "Unable to determine absolute path"   },
    { ERR_INVALID_TABWIDTH                   , "Invalid tabwidth value"              },
    { ERR_INVALID_FILETYPE                   , "Invalid filetype"                    },
    { ERR_INVALID_CONFIG_CHARACTERS          , "Invalid characters in config"        },
    { ERR_INVALID_CONFIG_SYNTAX              , "Invalid config syntax"               },
    { ERR_FAILED_TO_PARSE_CONFIG_FILE        , "Failed to parse config file"         },
    { ERR_FAILED_TO_PARSE_CONFIG_INPUT       , "Failed to parse config input"        },
    { ERR_INVALID_BLOCK_IDENTIFIER           , "Invalid block identifier"            },
    { ERR_EMPTY_BLOCK_DEFINITION             , "Empty block definition"              },
    { ERR_MISSING_VARIABLE_DEFINITION        , "Missing variable definition"         },
    { ERR_INVALID_STREAM                     , "Invalid stream"                      },
    { ERR_INVALID_ARGUMENTS                  , "Invalid arguments"                   },
    { ERR_INVALID_BUFFERPOS                  , "Invalid Buffer Position"             },
    { ERR_INVALID_REGEX                      , "Invalid Regex"                       },
    { ERR_REGEX_EXECUTION_FAILED             , "Regex execution failed"              },
    { ERR_TOO_MANY_REGEX_CAPTURE_GROUPS      , "Too many regex capture groups"       },
    { ERR_TOO_MANY_REGEX_BACKREFERENCES      , "Too many regex backreferences"       },
    { ERR_INVALID_CAPTURE_GROUP_BACKREFERENCE, "Invalid capture group backreference" }
};

Status st_get_error(ErrorCode error_code, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    Status status = st_get_custom_error(error_code, format, arg_ptr);
    va_end(arg_ptr);

    return status;
}

Status st_get_custom_error(ErrorCode error_code, const char *format, va_list arg_ptr)
{
    assert(!is_null_or_empty(format));

    char *error_msg = malloc(MAX_ERROR_MSG_SIZE);

    if (error_msg == NULL) {
        /* TODO Should/Can we raise an out of memory error here as well? */
        return STATUS_ERROR(error_code, st_get_default_error_message(error_code), 1);
    }

    vsnprintf(error_msg, MAX_ERROR_MSG_SIZE, format, arg_ptr);

    return STATUS_ERROR(error_code, error_msg, 0);
}

static char *st_get_default_error_message(ErrorCode error_code)
{
    size_t error_msg_num = sizeof(default_error_messages) / sizeof(ErrorCodeMsg);

    for (size_t k = 0; k < error_msg_num; k++) {
        if (default_error_messages[k].error_code == error_code) {
            return default_error_messages[k].error_msg;
        }
    }

    return "Unknown error occured";
}

void st_free_status(Status status)
{
    if (!status.msg_literal && status.msg != NULL) {
        free(status.msg);
    }
}

