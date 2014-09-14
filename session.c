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
#include "util.h"

Session *new_session(void)
{
    Session *sess = alloc(sizeof(Session));
    sess->buffers = NULL;
    sess->active_buffer = NULL;

    return sess;
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
