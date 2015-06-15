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
#include "session.h"
#include "status.h"
#include "util.h"
#include "buffer.h"
#include "command.h"
#include "config.h"

#define MAX_EMPTY_BUFFER_NAME_SIZE 20

static Status se_add_to_history(List *, char *);

Session *se_new(void)
{
    Session *sess = malloc(sizeof(Session));
    RETURN_IF_NULL(sess);
    memset(sess, 0, sizeof(Session));

    return sess;
}

int se_init(Session *sess, char *buffer_paths[], int buffer_num)
{
    if ((sess->error_buffer = bf_new_empty("errors")) == NULL) {
        return 0;
    }

    if ((sess->cmd_prompt.cmd_buffer = bf_new_empty("commands")) == NULL) {
        return 0;
    }

    if ((sess->msg_buffer = bf_new_empty("messages")) == NULL) {
        return 0;
    }

    if ((sess->search_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->replace_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->command_history = list_new()) == NULL) {
        return 0;
    }

    if (!cm_init_keymap(sess)) {
        return 0;
    }

    cf_set_config_session(sess);
    se_add_error(sess, cf_init_session_config(sess));

    for (int k = 1; k < buffer_num; k++) {
        se_add_error(sess, se_add_new_buffer(sess, buffer_paths[k]));
    }

    if (sess->buffer_num == 0) {
        se_add_new_empty_buffer(sess);
    }

    if (!se_set_active_buffer(sess, 0)) {
        return 0;
    }

    cf_set_buffer_var(sess->cmd_prompt.cmd_buffer, "linewrap", INT_VAL(0));

    sess->msgs_enabled = 1;

    return 1;
}

void se_free(Session *sess)
{
    if (sess == NULL) {
        return;
    }

    Buffer *buffer = sess->buffers;
    Buffer *tmp;

    while (buffer != NULL) {
        tmp = buffer->next;
        bf_free(buffer);
        buffer = tmp;
    }

    cm_free_keymap(sess);
    bf_free_textselection(&sess->clipboard);
    cf_free_config(sess->config);
    bf_free(sess->cmd_prompt.cmd_buffer);
    free(sess->cmd_prompt.cmd_text);
    bf_free(sess->error_buffer);
    bf_free(sess->msg_buffer);
    list_free_all(sess->search_history, NULL);
    list_free_all(sess->replace_history, NULL);
    list_free_all(sess->command_history, NULL);

    free(sess);
}

int se_add_buffer(Session *sess, Buffer *buffer)
{
    assert(buffer != NULL);

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

int se_set_active_buffer(Session *sess, size_t buffer_index)
{
    assert(sess->buffers != NULL);
    assert(buffer_index < sess->buffer_num);

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

Buffer *se_get_buffer(const Session *sess, size_t buffer_index)
{
    assert(sess->buffers != NULL);
    assert(buffer_index < sess->buffer_num);

    if (sess->buffers == NULL || buffer_index >= sess->buffer_num) {
        return NULL;
    }

    Buffer *buffer = sess->buffers;

    while (buffer_index-- != 0) {
        buffer = buffer->next;    
    }

    return buffer;
}

int se_remove_buffer(Session *sess, Buffer *to_remove)
{
    assert(sess->buffers != NULL);
    assert(to_remove != NULL);

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

    bf_free(buffer);

    return 1;
}

Status se_make_cmd_buffer_active(Session *sess, const char *prompt_text, List *history, int show_last_cmd)
{
    RETURN_IF_FAIL(se_update_cmd_prompt_text(sess, prompt_text));

    sess->cmd_prompt.cmd_buffer->next = sess->active_buffer;
    sess->active_buffer = sess->cmd_prompt.cmd_buffer;

    sess->cmd_prompt.cancelled = 0;
    sess->cmd_prompt.history = history;
    
    const char *cmd_text = NULL;
    
    if (history != NULL) {
        sess->cmd_prompt.history_index = list_size(history);

        if (show_last_cmd && sess->cmd_prompt.history_index > 0) {
            cmd_text = list_get(history, --sess->cmd_prompt.history_index); 
        }
    }

    RETURN_IF_FAIL(bf_set_text(sess->cmd_prompt.cmd_buffer, cmd_text));

    return bf_select_all_text(sess->cmd_prompt.cmd_buffer);
}

Status se_update_cmd_prompt_text(Session *sess, const char *text)
{
    assert(!is_null_or_empty(text));

    if (sess->cmd_prompt.cmd_text != NULL) {
        free(sess->cmd_prompt.cmd_text);
    }

    sess->cmd_prompt.cmd_text = strdupe(text);
    
    if (text != NULL && sess->cmd_prompt.cmd_text == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to set prompt text");
    }

    return STATUS_SUCCESS;
}

int se_end_cmd_buffer_active(Session *sess)
{
    assert(sess->active_buffer != NULL);

    if (sess->active_buffer == NULL) {
        return 0;
    }

    sess->active_buffer = sess->cmd_prompt.cmd_buffer->next;

    return 1;
}

int se_cmd_buffer_active(const Session *sess)
{
    assert(sess->active_buffer != NULL);

    if (sess->active_buffer == NULL) {
        return 0;
    }

    return sess->active_buffer == sess->cmd_prompt.cmd_buffer;
}

char *se_get_cmd_buffer_text(const Session *sess)
{
    return bf_to_string(sess->cmd_prompt.cmd_buffer);
}

void se_set_clipboard(Session *sess, TextSelection clipboard)
{
    if (sess->clipboard.str != NULL) {
        bf_free_textselection(&sess->clipboard);
    }

    sess->clipboard = clipboard;
}

void se_exclude_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types |= cmd_type;
}

void se_enable_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types &= ~cmd_type;
}

int se_command_type_excluded(const Session *sess, CommandType cmd_type)
{
    return sess->exclude_cmd_types & cmd_type;
}

int se_add_error(Session *sess, Status error)
{
    if (STATUS_IS_SUCCESS(error)) {
        return 0;
    }

    Buffer *error_buffer = sess->error_buffer;
    char error_msg[MAX_ERROR_MSG_SIZE];

    snprintf(error_msg, MAX_ERROR_MSG_SIZE, "Error %d: %s", error.error_code, error.msg);    
    st_free_status(error);

    if (!bp_at_buffer_start(&error_buffer->pos)) {
        bf_insert_character(error_buffer, "\n", 1);
    }

    bf_insert_string(error_buffer, error_msg, strnlen(error_msg, MAX_ERROR_MSG_SIZE), 1);

    return 1;
}

int se_has_errors(const Session *sess)
{
    return !bf_is_empty(sess->error_buffer);
}

void se_clear_errors(Session *sess)
{
    bf_clear(sess->error_buffer);
}

int se_add_msg(Session *sess, const char *msg)
{
    assert(!is_null_or_empty(msg));

    if (msg == NULL) {
        return 0; 
    } else if (!sess->msgs_enabled) {
        return 1;
    }

    Buffer *msg_buffer = sess->msg_buffer;

    if (!bp_at_buffer_start(&msg_buffer->pos)) {
        bf_insert_character(msg_buffer, "\n", 1);
    }

    bf_insert_string(msg_buffer, msg, strnlen(msg, MAX_MSG_SIZE), 1);

    return 1;
}

int se_has_msgs(const Session *sess)
{
    return !bf_is_empty(sess->msg_buffer);
}

void se_clear_msgs(Session *sess)
{
    bf_clear(sess->msg_buffer);
}

Status se_add_new_buffer(Session *sess, const char *file_path)
{
    if (file_path == NULL || strnlen(file_path, 1) == 0) {
        return st_get_error(ERR_INVALID_FILE_PATH, "Invalid file path - \"%s\"", file_path);
    }

    FileInfo file_info;
    Buffer *buffer = NULL;
    Status status;

    RETURN_IF_FAIL(fi_init(&file_info, file_path));

    if (fi_is_directory(&file_info)) {
        status = st_get_error(ERR_FILE_IS_DIRECTORY, "%s is a directory", file_info.file_name);
        goto cleanup;
    } else if (fi_is_special(&file_info)) {
        status = st_get_error(ERR_FILE_IS_SPECIAL, "%s is not a regular file", file_info.file_name);
        goto cleanup;
    }

    buffer = bf_new(&file_info);

    if (buffer == NULL) {
        status = st_get_error(ERR_OUT_OF_MEMORY, 
                           "Out of memory - Unable to "
                           "create buffer for file %s", 
                           file_info.file_name);
        goto cleanup;
    }

    status = bf_load_file(buffer);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    se_add_buffer(sess, buffer);

    return STATUS_SUCCESS;

cleanup:
    fi_free(&file_info);
    bf_free(buffer);

    return status;
}

Status se_add_new_empty_buffer(Session *sess)
{
    char empty_buf_name[MAX_EMPTY_BUFFER_NAME_SIZE];
    snprintf(empty_buf_name, MAX_EMPTY_BUFFER_NAME_SIZE, "[new %zu]", ++sess->empty_buffer_num);

    Buffer *buffer = bf_new_empty(empty_buf_name);

    if (buffer == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, 
                         "Out of memory - Unable to "
                         "create empty buffer");
    }   

    se_add_buffer(sess, buffer);

    return STATUS_SUCCESS;
}

Status se_get_buffer_index(const Session *sess, const char *file_path, int *buffer_index_ptr)
{
    assert(!is_null_or_empty(file_path));
    assert(buffer_index_ptr != NULL);

    FileInfo file_info;
    RETURN_IF_FAIL(fi_init(&file_info, file_path));

    Buffer *buffer = sess->buffers;
    *buffer_index_ptr = -1;
    int buffer_index = 0;

    while (buffer != NULL) {
        if (fi_equal(&buffer->file_info, &file_info)) {
            *buffer_index_ptr = buffer_index; 
            break;
        } 

        buffer = buffer->next;
        buffer_index++;
    }

    fi_free(&file_info);

    return STATUS_SUCCESS;
}

static Status se_add_to_history(List *history, char *text)
{
    assert(!is_null_or_empty(text));

    size_t size = list_size(history);

    if (size > 0 && strcmp(list_get(history, size - 1), text) == 0) {
        return STATUS_SUCCESS;
    }

    if (!list_add(history, text)) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable save search history");
    }

    return STATUS_SUCCESS;
}

Status se_add_search_to_history(Session *sess, char *search_text)
{
    return se_add_to_history(sess->search_history, search_text);
}

Status se_add_replace_to_history(Session *sess, char *replace_text)
{
    return se_add_to_history(sess->replace_history, replace_text);
}

Status se_add_cmd_to_history(Session *sess, char *cmd_text)
{
    return se_add_to_history(sess->command_history, cmd_text);
}
