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

#define RETURN_IF_NULL(ptr) if ((ptr) == NULL) { return NULL; }
#define STATUS_ERROR(ecode, emsg, emliteral) (Status)\
                    { .error_code = (ecode), .error_msg = (emsg), .error_msg_literal = (emliteral) }
#define STATUS_SUCCESS STATUS_ERROR(ERR_NONE, NULL, 0)
#define STATUS_IS_SUCCESS(status) ((status).error_code == ERR_NONE)
#define RETURN_IF_FAIL(status) { Status _wed_status = (status);\
                               if (!STATUS_IS_SUCCESS(_wed_status)) return _wed_status; }

#define ERROR_QUEUE_MAX_SIZE 10
#define MAX_ERROR_MSG_SIZE 1024

typedef enum {
    ERR_NONE,
    ERR_FILE_DOESNT_EXIST,
    ERR_FILE_IS_DIRECTORY,
    ERR_UNABLE_TO_OPEN_FILE,
    ERR_UNABLE_TO_READ_FILE,
    ERR_INVALID_COMMAND,
    ERR_INVALID_CHARACTER,
    ERR_INVALID_STRING,
    ERR_INVALID_VAR,
    ERR_INVALID_VAL,
    ERR_INVALID_CONFIG_ENTRY,
    ERR_INVALID_FILE_PATH,
    ERR_OUT_OF_MEMORY
} ErrorCode;

/* Used to determine the success of an action.
 * error is NULL when successful. */
typedef struct {
    ErrorCode error_code;
    char *error_msg;
    int error_msg_literal;
} Status;

/* Queue structure for storing multiple errors */
typedef struct {
    Status errors[ERROR_QUEUE_MAX_SIZE];
    int start;
    int count;
} ErrorQueue;

Status get_error(ErrorCode, char *, ...);
int error_queue_full(ErrorQueue *);
int error_queue_empty(ErrorQueue *);
int error_queue_add(ErrorQueue *, Status);
Status error_queue_remove(ErrorQueue *);
void free_error_queue(ErrorQueue *);
void free_error(Status);
char *get_error_msg(Status);

#endif

