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

#include "session.h"
#include "status.h"
#include "util.h"
#include "command.h"

Session *new_session(void)
{
    Session *sess = alloc(sizeof(Session));
    sess->buffers = NULL;
    sess->active_buffer = NULL;
    sess->keymap = NULL;

    return sess;
}

int init_session(Session *sess, char *buffer_paths[], int buffer_num)
{
    FileInfo file_info;

    /* Limited to one file for the moment */
    if (buffer_num > 2) {
        buffer_num = 2;
    }

    for (int k = 1; k < buffer_num; k++) {
        init_fileinfo(&file_info, buffer_paths[k]);

        if (file_info.is_directory) {
            free_fileinfo(file_info);
            add_error(sess, raise_param_error(ERR_FILE_IS_DIRECTORY, STR_VAL(file_info.file_name)));
            continue;
        }

        Buffer *buffer = new_buffer(file_info);
        Status load_status = load_buffer(buffer);

        if (add_error(sess, load_status)) {
            free_buffer(buffer);
            continue;
        }

        add_buffer(sess, buffer);
    }

    if (get_buffer_num(sess) == 0) {
        add_buffer(sess, new_empty_buffer()); 
    }

    if (!set_active_buffer(sess, 0)) {
        return 0;
    }

    init_keymap(sess);

    return 1;
}

void free_session(Session *sess)
{
    if (sess == NULL) {
        return;
    }

    Buffer *buffer = sess->buffers;
    Buffer *tmp = NULL;

    while (buffer != NULL) {
        tmp = buffer->next;
        free_buffer(buffer);
        buffer = tmp;
    }

    free_error_queue(&sess->error_queue);
    free_hashmap(sess->keymap);

    free(sess);
}

int add_buffer(Session *sess, Buffer *buffer)
{
    if (sess == NULL || buffer == NULL) {
        return 0;
    }

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

size_t get_buffer_num(Session *sess)
{
    if (sess == NULL || sess->buffers == NULL) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    size_t buffer_num = 1;

    while ((buffer = buffer->next) != NULL) {
        buffer_num++;
    }

    return buffer_num;
}

int set_active_buffer(Session *sess, size_t buff_index)
{
    if (sess == NULL || sess->buffers == NULL) {
        return 0;
    }

    size_t buffer_num = get_buffer_num(sess);

    if (buff_index >= buffer_num) {
        return 0;
    }

    Buffer *buffer = sess->buffers;

    while (buff_index-- != 0) {
         buffer = buffer->next;
    }

    sess->active_buffer = buffer;

    return 1;
}

int remove_buffer(Session *sess, Buffer *to_remove)
{
    if (sess == NULL || sess->buffers == NULL || to_remove == NULL) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    Buffer *prev = NULL;

    while (buffer != NULL && to_remove != buffer) {
        prev = buffer;    
        buffer = buffer->next;
    }

    if (buffer == NULL) {
        return 0;
    }

    if (prev != NULL) {
        if (buffer->next != NULL) {
            prev->next = buffer->next; 
        } else {
            prev->next = NULL;
        }
    }

    if (sess->active_buffer == buffer) {
        if (buffer->next != NULL) {
            sess->active_buffer = buffer->next;
        } else if (prev != NULL) {
            sess->active_buffer = prev;
        } else {
            sess->buffers = new_empty_buffer();
            sess->active_buffer = sess->buffers;
        } 
    }

    free_buffer(buffer);

    return 1;
}

int add_err(Session *sess, Error *error)
{
    if (sess == NULL || error == NULL) {
        return 0;
    }

    return error_queue_add(&sess->error_queue, error);
}

int add_error(Session *sess, Status status)
{
    if (is_success(status)) {
        return 0;
    } 

    add_err(sess, status.error);

    return 1;
}
    
