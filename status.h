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
#include "variable.h"

#define SUCCESS_CODE 1
#define FAIL_CODE 0
#define STATUS(code) (Status) { .success = (code), .error = NULL }
#define STATUS_SUCCESS STATUS(SUCCESS_CODE)
#define STATUS_FAIL STATUS(FAIL_CODE)

typedef enum {
    ERR_SVR_MINIMAL,
    ERR_SVR_WARNING,
    ERR_SVR_CRITICAL
} ErrorSeverity;

typedef enum {
    ERR_INVALID_ERROR_CODE,
    ERR_FILE_DOESNT_EXIST,
    ERR_FILE_IS_DIRECTORY,
    ERR_LAST_ENTRY
} ErrorCode;

/* Error info is stored in an Error structure */
typedef struct {
    ErrorCode error_code; /* Which error this is */
    ErrorSeverity error_svr; /* How serious is this error */
    int accepts_param; /* Does this error message allow parameters to be set through format specifiers */
    char *msg; /* The core error message */
    Value param; /* Error instance specific message part */
} Error;

/* Used to determine the success of an action.
 * error is NULL when successful. */
typedef struct {
    int success;
    Error *error;
} Status;

int is_success(Status);
Status raise_error(ErrorCode);
Status raise_param_error(ErrorCode, Value);

#endif

