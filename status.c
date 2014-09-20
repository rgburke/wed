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

#include <string.h>
#include "status.h"
#include "variable.h"
#include "util.h"

static const Error errors[] = {
    { ERR_INVALID_ERROR_CODE , ERR_SVR_CRITICAL, 1, "Invalid Error code %d"      , INT_VAL_STRUCT(0)  },
    { ERR_FILE_DOESNT_EXIST  , ERR_SVR_CRITICAL, 1, "File %s doesn't exist"      , STR_VAL_STRUCT("") },
    { ERR_FILE_IS_DIRECTORY  , ERR_SVR_CRITICAL, 1, "File %s is a directory"     , STR_VAL_STRUCT("") },
    { ERR_UNABLE_TO_OPEN_FILE, ERR_SVR_CRITICAL, 1, "Unable to open file %s"     , STR_VAL_STRUCT("") },
    { ERR_UNABLE_TO_READ_FILE, ERR_SVR_CRITICAL, 1, "Unable to read from file %s", STR_VAL_STRUCT("") },
    { ERR_INVALID_COMMAND    , ERR_SVR_CRITICAL, 1, "Invalid command code %d"    , INT_VAL_STRUCT(0)  }
};

int is_success(Status status)
{
    return status.success == SUCCESS_CODE;
}

Status raise_error(ErrorCode error_code)
{
    Value param = INT_VAL(0);
    return raise_param_error(error_code, param);
}

Status raise_param_error(ErrorCode error_code, Value param)
{
    if (error_code < 0 || error_code >= ERR_LAST_ENTRY) {
        param.type = VAL_TYPE_INT;
        param.val.ival = error_code; 
        error_code = 0;
    }

    Error *error = alloc(sizeof(Error));
    memcpy(error, &errors[error_code], sizeof(Error));
    error->param = deep_copy_value(param);

    Status status = { .success = 0, .error = error };
    return status;
}

void free_error(Error *error)
{
    if (error == NULL) {
        return;
    }

    free_value(error->param);
    free(error);
}

int error_queue_full(ErrorQueue *error_queue)
{
    return error_queue->count == ERROR_QUEUE_MAX_SIZE; 
}

int error_queue_empty(ErrorQueue *error_queue)
{
    return error_queue->count == 0;
}

int error_queue_add(ErrorQueue *error_queue, Error *error)
{
    if (error_queue_full(error_queue)) {
        return 0;
    } 

    int index = (error_queue->start + error_queue->count++) % ERROR_QUEUE_MAX_SIZE;
    error_queue->errors[index] = error;

    return 1;
}

Error *error_queue_remove(ErrorQueue *error_queue)
{
    if (error_queue_empty(error_queue)) {
        return NULL;
    }

    Error *error = error_queue->errors[error_queue->start++];
    error_queue->start %= ERROR_QUEUE_MAX_SIZE; 
    error_queue->count--;

    return error;
}

void free_error_queue(ErrorQueue *error_queue)
{
    while (!error_queue_empty(error_queue)) {
        free_error(error_queue_remove(error_queue));
    }
}

