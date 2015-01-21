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
#include "status.h"
#include "variable.h"
#include "util.h"

static char *get_default_error_message(ErrorCode);

typedef struct {
    ErrorCode error_code;
    char *error_msg;
} ErrorCodeMsg;

static const ErrorCodeMsg default_error_messages[] = {
    { ERR_FILE_DOESNT_EXIST   , "File doesn't exist"       },
    { ERR_FILE_IS_DIRECTORY   , "File is a directory"      },
    { ERR_UNABLE_TO_OPEN_FILE , "Unable to open file"      },
    { ERR_UNABLE_TO_READ_FILE , "Unable to read from file" },
    { ERR_INVALID_COMMAND     , "Invalid command"          },
    { ERR_INVALID_CHARACTER   , "Invalid character"        },
    { ERR_INVALID_STRING      , "Invalid string"           },
    { ERR_INVALID_VAR         , "Invalid variable"         },
    { ERR_INVALID_VAL         , "Invalid value"            },
    { ERR_INVALID_CONFIG_ENTRY, "Invalid config entry"     },
    { ERR_INVALID_FILE_PATH   , "Invalid file path"        },
    { ERR_OUT_OF_MEMORY       , "Out of memory"            }
};

Status get_error(ErrorCode error_code, char *format, ...)
{
    char *error_msg = malloc(MAX_ERROR_MSG_SIZE);

    if (error_msg == NULL) {
        /* TODO Should/Can we raise an out of memory error here as well? */
        return STATUS_ERROR(error_code, get_default_error_message(error_code), 1);
    }

    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsnprintf(error_msg, MAX_ERROR_MSG_SIZE, format, arg_ptr);
    va_end(arg_ptr);

    return STATUS_ERROR(error_code, error_msg, 0);
}

int error_queue_full(ErrorQueue *error_queue)
{
    return error_queue->count == ERROR_QUEUE_MAX_SIZE; 
}

int error_queue_empty(ErrorQueue *error_queue)
{
    return error_queue->count == 0;
}

static char *get_default_error_message(ErrorCode error_code)
{
    size_t error_msg_num = sizeof(default_error_messages) / sizeof(ErrorCodeMsg);

    for (size_t k = 0; k < error_msg_num; k++) {
        if (default_error_messages[k].error_code == error_code) {
            return default_error_messages[k].error_msg;
        }
    }

    return "Unknown error occured";
}

int error_queue_add(ErrorQueue *error_queue, Status status)
{
    if (STATUS_IS_SUCCESS(status) || error_queue_full(error_queue)) {
        return 0;
    } 

    int index = (error_queue->start + error_queue->count++) % ERROR_QUEUE_MAX_SIZE;
    error_queue->errors[index] = status;

    return 1;
}

Status error_queue_remove(ErrorQueue *error_queue)
{
    if (error_queue_empty(error_queue)) {
        return STATUS_SUCCESS;
    }

    Status error = error_queue->errors[error_queue->start++];
    error_queue->start %= ERROR_QUEUE_MAX_SIZE; 
    error_queue->count--;

    return error;
}

void free_error_queue(ErrorQueue *error_queue)
{
    if (error_queue == NULL) {
        return;
    }

    Status error;

    while (!error_queue_empty(error_queue)) {
        error = error_queue_remove(error_queue);
        free_error(error);
    }
}

void free_error(Status error)
{
    if (!STATUS_IS_SUCCESS(error) && !error.error_msg_literal) {
        free(error.error_msg);
    }
}

char *get_error_msg(Status error) {
    if (STATUS_IS_SUCCESS(error)) {
        return NULL;
    }

    char *error_msg = alloc(MAX_ERROR_MSG_SIZE);
    snprintf(error_msg, MAX_ERROR_MSG_SIZE, "Error %d: %s", error.error_code, error.error_msg);    
    return error_msg;
}
