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
    { ERR_INVALID_ERROR_CODE, ERR_SVR_CRITICAL, 1, "Invalid Error code %d" , INT_VAL(0)  },
    { ERR_FILE_DOESNT_EXIST , ERR_SVR_CRITICAL, 1, "File %s doesn't exist" , STR_VAL("") },
    { ERR_FILE_IS_DIRECTORY , ERR_SVR_CRITICAL, 1, "File %s is a directory", STR_VAL("") }
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
    error->param = param;

    Status status = { .success = 0, .error = error };
    return status;
}

