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
#include "session.h"
#include "status.h"
#include "util.h"
#include "buffer.h"
#include "command.h"
#include "config.h"

#define MAX_EMPTY_BUFFER_NAME_SIZE 20

Session *new_session(void)
{
    Session *sess = malloc(sizeof(Session));
    RETURN_IF_NULL(sess);
    memset(sess, 0, sizeof(Session));

    return sess;
}

int init_session(Session *sess, char *buffer_paths[], int buffer_num)
{
    if ((sess->error_buffer = new_empty_buffer("errors")) == NULL) {
        return 0;
    }

    if ((sess->cmd_prompt.cmd_buffer = new_empty_buffer("commands")) == NULL) {
        return 0;
    }

    if ((sess->msg_buffer = new_empty_buffer("messages")) == NULL) {
        return 0;
    }

    if (!init_keymap(sess)) {
        return 0;
    }

    set_config_session(sess);
    add_error(sess, init_session_config(sess));

    for (int k = 1; k < buffer_num; k++) {
        add_error(sess, add_new_buffer(sess, buffer_paths[k]));
    }

    if (sess->buffer_num == 0) {
        add_new_empty_buffer(sess);
    }

    if (!set_active_buffer(sess, 0)) {
        return 0;
    }

    set_buffer_var(sess->cmd_prompt.cmd_buffer, "linewrap", "0");

    return 1;
}

void free_session(Session *sess)
{
    if (sess == NULL) {
        return;
    }

    Buffer *buffer = sess->buffers;
    Buffer *tmp;

    while (buffer != NULL) {
        tmp = buffer->next;
        free_buffer(buffer);
        buffer = tmp;
    }

    free_keymap(sess);
    free_textselection(sess->clipboard);
    free_config(sess->config);
    free_buffer(sess->cmd_prompt.cmd_buffer);
    free(sess->cmd_prompt.cmd_text);
    free_buffer(sess->error_buffer);
    free_buffer(sess->msg_buffer);

    free(sess);
}

int add_buffer(Session *sess, Buffer *buffer)
{
    if (buffer == NULL) {
        return 0;
    }

    sess->buffer_num++;

    if (sess->buffers == NULL) {
        sess->buffers = buffer;
        return 1;
    }

    Buffer *buff = sess->buffers;

    do {
        if (buff->next == NULL) {
            buff->next = buffer;
            break;
        }

        buff = buff->next;
    } while (1);
    
    return 1;
}

int set_active_buffer(Session *sess, size_t buffer_index)
{
    if (sess->buffers == NULL || buffer_index >= sess->buffer_num) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    size_t iter = 0;

    while (iter < buffer_index) {
         buffer = buffer->next;
         iter++;
    }

    sess->active_buffer = buffer;
    sess->active_buffer_index = buffer_index;

    return 1;
}

Buffer *get_buffer(Session *sess, size_t buffer_index)
{
    if (sess->buffers == NULL || buffer_index >= sess->buffer_num) {
        return NULL;
    }

    Buffer *buffer = sess->buffers;

    while (buffer_index-- != 0) {
        buffer = buffer->next;    
    }

    return buffer;
}

int remove_buffer(Session *sess, Buffer *to_remove)
{
    if (sess->buffers == NULL || to_remove == NULL) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    Buffer *prev = NULL;
    size_t buffer_index = 0;

    while (buffer != NULL && to_remove != buffer) {
        prev = buffer;    
        buffer = buffer->next;
        buffer_index++;
    }

    if (buffer == NULL) {
        return 0;
    }

    if (prev != NULL) {
        if (buffer->next != NULL) {
            prev->next = buffer->next; 

            if (sess->active_buffer_index == buffer_index) {
                sess->active_buffer = buffer->next;
            }
        } else {
            prev->next = NULL;
            sess->active_buffer = prev;

            if (sess->active_buffer_index == buffer_index) {
                sess->active_buffer_index--;
            }
        }
    } else if (sess->active_buffer == buffer) {
        if (buffer->next != NULL) {
            sess->buffers = buffer->next;
            sess->active_buffer = sess->buffers;
        } else {
            sess->buffers = NULL;
            sess->active_buffer = NULL;
        } 
    }

    sess->buffer_num--;

    free_buffer(buffer);

    return 1;
}

Status make_cmd_buffer_active(Session *sess, const char *text)
{
    sess->cmd_prompt.cmd_buffer->next = sess->active_buffer;
    sess->active_buffer = sess->cmd_prompt.cmd_buffer;

    if (sess->cmd_prompt.cmd_text != NULL) {
        free(sess->cmd_prompt.cmd_text);
    }

    sess->cmd_prompt.cmd_text = strdupe(text);
    
    if (text != NULL && sess->cmd_prompt.cmd_text == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to set prompt text");
    }

    sess->cmd_prompt.cancelled = 0;
    return clear_buffer(sess->cmd_prompt.cmd_buffer);
}

int end_cmd_buffer_active(Session *sess)
{
    if (sess->active_buffer == NULL || 
        sess->cmd_prompt.cmd_buffer == NULL) {
        return 0;
    }

    sess->active_buffer = sess->cmd_prompt.cmd_buffer->next;

    return 1;
}

int cmd_buffer_active(Session *sess)
{
    if (sess->active_buffer == NULL || 
        sess->cmd_prompt.cmd_buffer == NULL) {
        return 0;
    }

    return sess->active_buffer == sess->cmd_prompt.cmd_buffer;
}

char *get_cmd_buffer_text(Session *sess)
{
    return get_buffer_as_string(sess->cmd_prompt.cmd_buffer);
}

void set_clipboard(Session *sess, TextSelection *clipboard)
{
    if (sess->clipboard != NULL) {
        free_textselection(sess->clipboard);
    }

    sess->clipboard = clipboard;
}

void exclude_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types |= cmd_type;
}

void enable_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types &= ~cmd_type;
}

int command_type_excluded(Session *sess, CommandType cmd_type)
{
    return sess->exclude_cmd_types & cmd_type;
}

int add_error(Session *sess, Status error)
{
    if (STATUS_IS_SUCCESS(error)) {
        return 0;
    }

    Buffer *error_buffer = sess->error_buffer;
    char error_msg[MAX_ERROR_MSG_SIZE];

    snprintf(error_msg, MAX_ERROR_MSG_SIZE, "Error %d: %s", error.error_code, error.msg);    
    free_status(error);

    if (!bufferpos_at_buffer_start(error_buffer->pos)) {
        insert_line(error_buffer);
    }

    insert_string(error_buffer, error_msg, strnlen(error_msg, MAX_ERROR_MSG_SIZE), 1);

    return 1;
}

int has_errors(Session *sess)
{
    return !buffer_is_empty(sess->error_buffer);
}

void clear_errors(Session *sess)
{
    clear_buffer(sess->error_buffer);
}

int add_msg(Session *sess, const char *msg)
{
    if (msg == NULL) {
        return 0; 
    }

    Buffer *msg_buffer = sess->msg_buffer;

    if (!bufferpos_at_buffer_start(msg_buffer->pos)) {
        insert_line(msg_buffer);
    }

    insert_string(msg_buffer, msg, strnlen(msg, MAX_MSG_SIZE), 1);

    return 1;
}

int has_msgs(Session *sess)
{
    return !buffer_is_empty(sess->msg_buffer);
}

void clear_msgs(Session *sess)
{
    clear_buffer(sess->msg_buffer);
}

Status add_new_buffer(Session *sess, const char *file_path)
{
    if (file_path == NULL || strnlen(file_path, 1) == 0) {
        return get_error(ERR_INVALID_FILE_PATH, "Invalid file path - \"%s\"", file_path);
    }

    FileInfo file_info;
    Buffer *buffer = NULL;
    Status status;

    RETURN_IF_FAIL(init_fileinfo(&file_info, file_path));

    if (file_is_directory(file_info)) {
        status = get_error(ERR_FILE_IS_DIRECTORY, "%s is a directory", file_info.file_name);
        goto cleanup;
    } else if (file_is_special(file_info)) {
        status = get_error(ERR_FILE_IS_SPECIAL, "%s is not a regular file", file_info.file_name);
        goto cleanup;
    }

    buffer = new_buffer(file_info);

    if (buffer == NULL) {
        status = get_error(ERR_OUT_OF_MEMORY, 
                           "Out of memory - Unable to "
                           "create buffer for file %s", 
                           file_info.file_name);
        goto cleanup;
    }

    status = load_buffer(buffer);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    add_buffer(sess, buffer);

    return STATUS_SUCCESS;

cleanup:
    free_fileinfo(file_info);
    free_buffer(buffer);

    return status;
}

Status add_new_empty_buffer(Session *sess)
{
    char empty_buf_name[MAX_EMPTY_BUFFER_NAME_SIZE];
    snprintf(empty_buf_name, MAX_EMPTY_BUFFER_NAME_SIZE, "[new %zu]", ++sess->empty_buffer_num);

    Buffer *buffer = new_empty_buffer(empty_buf_name);

    if (buffer == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, 
                         "Out of memory - Unable to "
                         "create empty buffer");
    }   

    add_buffer(sess, buffer);

    return STATUS_SUCCESS;
}

Status get_buffer_index(Session *sess, const char *file_path, int *buffer_index_ptr)
{
    if (file_path == NULL || strnlen(file_path, 1) == 0 || buffer_index_ptr == NULL) {
        return STATUS_SUCCESS;
    }

    FileInfo file_info;
    RETURN_IF_FAIL(init_fileinfo(&file_info, file_path));

    Buffer *buffer = sess->buffers;
    *buffer_index_ptr = -1;
    int buffer_index = 0;

    while (buffer != NULL) {
        if (file_info_equal(buffer->file_info, file_info)) {
            *buffer_index_ptr = buffer_index; 
            break;
        } 

        buffer = buffer->next;
        buffer_index++;
    }

    free_fileinfo(file_info);

    return STATUS_SUCCESS;
}
