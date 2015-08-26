/*
 * Copyright (C) 2015 Richard Burke
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
#include <ctype.h>
#include <assert.h>
#include "undo.h"
#include "buffer.h"
#include "util.h"

#define LIST_CHILDREN_INIT 4

static TextChange *bc_tc_new(TextChangeType, const char *, size_t, const BufferPos *);
static void bc_tc_free(TextChange *);
static Status bc_add_text_change_to_prev(BufferChanges *, TextChangeType, const char *, size_t, const BufferPos *, int *);
static Status bc_add_text_change(BufferChanges *, TextChangeType, const char *, size_t, const BufferPos *);
static BufferChange *bc_new(BufferChangeType, Change);
static void bc_free_change(BufferChangeType, Change);
static void bc_free_buffer_change(BufferChange *);
static void bc_free_stack(BufferChange *);
static Status bc_add_change(BufferChanges *, BufferChangeType, Change);
static Status bc_apply(BufferChange *, Buffer *, int);
static Status bc_tc_apply(TextChange *, Buffer *, int);

void bc_init(BufferChanges *changes)
{
    assert(changes != NULL);
    memset(changes, 0, sizeof(BufferChanges));
    changes->accept_new_changes = 1;
}

void bc_free(BufferChanges *changes)
{
    bc_free_stack(changes->undo);
    bc_free_stack(changes->redo);
}

static TextChange *bc_tc_new(TextChangeType change_type, const char *str, 
                             size_t str_len, const BufferPos *pos)
{
    TextChange *text_change = malloc(sizeof(TextChange));
    RETURN_IF_NULL(text_change);

    memset(text_change, 0, sizeof(TextChange));

    if (change_type == TCT_DELETE) {
        text_change->str = strdupe(str);

        if (text_change->str == NULL) {
            free(text_change);
            return NULL;
        }
    }

    text_change->change_type = change_type;
    text_change->str_len = str_len;
    text_change->pos = *pos;

    return text_change;
}

static void bc_tc_free(TextChange *text_change)
{
    if (text_change == NULL) {
        return;
    }

    free(text_change->str);
    free(text_change);
}

static Status bc_add_text_change_to_prev(BufferChanges *changes, TextChangeType change_type, 
                                         const char *str, size_t str_len, const BufferPos *pos,
                                         int *added_to_prev_change)
{
    *added_to_prev_change = 0;

    if (changes->undo == NULL ||
        changes->undo->change_type != BCT_TEXT_CHANGE) {
        return STATUS_SUCCESS;
    }

    TextChange *prev_change = changes->undo->change.text_change; 

    if (prev_change->change_type != change_type) {
        return STATUS_SUCCESS;
    }

    int add_to_prev = 0;

    if (change_type == TCT_INSERT) {
        if (prev_change->pos.offset + prev_change->str_len == pos->offset) {
            add_to_prev = 1;

            if (prev_change->str_len > 1) {
                char last_char = gb_get_at(prev_change->pos.data, 
                                           prev_change->pos.offset + 
                                           prev_change->str_len - 1);
                char next_char = gb_get_at(pos->data, pos->offset);

                if (isspace(last_char) && !isspace(next_char)) {
                    add_to_prev = 0;
                }
            }
        }
    } else if (change_type == TCT_DELETE) {
        add_to_prev = (prev_change->pos.offset == pos->offset);
    }

    if (add_to_prev) {
        if (change_type == TCT_INSERT) {
            prev_change->str_len += str_len;
        } else if (change_type == TCT_DELETE) {
            size_t new_str_len = prev_change->str_len + str_len;
            char *new_str = realloc(prev_change->str, new_str_len);

            if (new_str == NULL) {
                return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                        "Unable to save undo history");
            }

            prev_change->str = new_str;
            memcpy(prev_change->str + prev_change->str_len, str, str_len);
            prev_change->str_len = new_str_len;
        }

        *added_to_prev_change = 1;
    }

    return STATUS_SUCCESS;
}

Status bc_add_text_insert(BufferChanges *changes, size_t str_len, 
                          const BufferPos *pos)
{
    return bc_add_text_change(changes, TCT_INSERT, NULL, str_len, pos);
}

Status bc_add_text_delete(BufferChanges *changes, const char *str, 
                          size_t str_len, const BufferPos *pos)
{
    assert(str != NULL);
    return bc_add_text_change(changes, TCT_DELETE, str, str_len, pos);
}

static Status bc_add_text_change(BufferChanges *changes, TextChangeType change_type, 
                                 const char *str, size_t str_len, const BufferPos *pos)
{
    assert(pos != NULL);
     
    if (str_len == 0 || !changes->accept_new_changes) {
        return STATUS_SUCCESS;
    }

    int added_to_prev_change;
    Status status = bc_add_text_change_to_prev(changes, change_type, str, 
                                               str_len, pos, &added_to_prev_change);

    if (!STATUS_IS_SUCCESS(status) || added_to_prev_change) {
        return status;
    }

    TextChange *text_change = bc_tc_new(change_type, str, str_len, pos);

    if (text_change == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to save text change");
    }

    Change change = { .text_change = text_change };

    return bc_add_change(changes, BCT_TEXT_CHANGE, change);
}

static BufferChange *bc_new(BufferChangeType change_type, Change change)
{
    BufferChange *buffer_change = malloc(sizeof(BufferChange));
    RETURN_IF_NULL(buffer_change);

    memset(buffer_change, 0, sizeof(BufferChange));
    buffer_change->change_type = change_type;
    buffer_change->change = change;

    return buffer_change;
}

static void bc_free_change(BufferChangeType change_type, Change change)
{
    switch (change_type) {
        case BCT_TEXT_CHANGE: 
            {
                bc_tc_free(change.text_change);
                break;
            }
        default:
            {
                break;
            }
    }
}

static void bc_free_buffer_change(BufferChange *buffer_change)
{
    if (buffer_change == NULL) {
        return;
    }

    bc_free_change(buffer_change->change_type, buffer_change->change);

    if (buffer_change->children != NULL) {
        BufferChange *child;

        for (size_t k = 0; k < list_size(buffer_change->children); k++) {
            child = list_get(buffer_change->children, k);
            bc_free_buffer_change(child);
        }

        list_free(buffer_change->children);
    }

    free(buffer_change);
}

static void bc_free_stack(BufferChange *buffer_change)
{
    BufferChange *next;

    while (buffer_change != NULL) {
        next = buffer_change->next;
        bc_free_buffer_change(buffer_change);
        buffer_change = next;
    }
}

static Status bc_add_change(BufferChanges *changes, BufferChangeType change_type, Change change)
{
    BufferChange *buffer_change = bc_new(change_type, change);    

    if (buffer_change == NULL) {
        bc_free_change(change_type, change);
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to save buffer change");
    }

    if (changes->group_changes) {
        assert(changes->undo != NULL);
        assert(changes->undo->change_type == BCT_GROUPED_CHANGE);

        if (!list_add(changes->undo->children, buffer_change)) {
            return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                "Unable to save buffer change");
        }
    } else {
        if (changes->undo != NULL) {
            buffer_change->next = changes->undo;
        }

        changes->undo = buffer_change;
    }

    bc_free_stack(changes->redo);
    changes->redo = NULL;

    return STATUS_SUCCESS;
}

int bc_can_undo(const BufferChanges *changes)
{
    return changes->undo != NULL;
}

int bc_can_redo(const BufferChanges *changes)
{
    return changes->redo != NULL;
}

int bc_grouped_changes_started(const BufferChanges *changes)
{
    return changes->group_changes;
}

Status bc_start_grouped_changes(BufferChanges *changes)
{
    assert(!changes->group_changes);

    if (changes->group_changes) {
        return STATUS_SUCCESS;
    }

    List *children = list_new_prealloc(LIST_CHILDREN_INIT);

    if (children == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to save buffer changes");
    }

    Change change = { NULL };
    RETURN_IF_FAIL(bc_add_change(changes, BCT_GROUPED_CHANGE, change));
    
    BufferChange *buffer_change = changes->undo;
    buffer_change->children = children;

    changes->group_changes = 1;

    return STATUS_SUCCESS;
}

Status bc_end_grouped_changes(BufferChanges *changes)
{
    assert(changes->group_changes);
    changes->group_changes = 0;

    assert(changes->undo != NULL);
    assert(changes->undo->change_type == BCT_GROUPED_CHANGE);

    if (changes->undo == NULL ||
        changes->undo->change_type != BCT_GROUPED_CHANGE) {
        return STATUS_SUCCESS;
    }

    BufferChange *buffer_change = changes->undo;

    if (list_size(buffer_change->children) == 0) {
        changes->undo = buffer_change->next;
        bc_free_buffer_change(buffer_change);
    }

    return STATUS_SUCCESS;
}

Status bc_undo(BufferChanges *changes, Buffer *buffer)
{
    if (!bc_can_undo(changes)) {
        return STATUS_SUCCESS;
    }

    BufferChange *buffer_change = changes->undo;

    changes->accept_new_changes = 0;
    Status status = bc_apply(buffer_change, buffer, 0);
    changes->accept_new_changes = 1;

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    changes->undo = buffer_change->next;
    buffer_change->next = changes->redo;
    changes->redo = buffer_change;

    return STATUS_SUCCESS;
}

Status bc_redo(BufferChanges *changes, Buffer *buffer)
{
    if (!bc_can_redo(changes)) {
        return STATUS_SUCCESS;
    }

    BufferChange *buffer_change = changes->redo;

    changes->accept_new_changes = 0;
    Status status = bc_apply(buffer_change, buffer, 1);
    changes->accept_new_changes = 1;

    if (!STATUS_IS_SUCCESS(status)) {
        return status;
    }

    changes->redo = buffer_change->next;
    buffer_change->next = changes->undo;
    changes->undo = buffer_change;

    return STATUS_SUCCESS;
}

static Status bc_apply(BufferChange *buffer_change, Buffer *buffer, int redo)
{
    Status status = STATUS_SUCCESS;

    switch (buffer_change->change_type) {
        case BCT_TEXT_CHANGE:
            {
                status = bc_tc_apply(buffer_change->change.text_change, buffer, redo);
                break;
            }
        case BCT_GROUPED_CHANGE:
            {
                if (redo) {
                    for (size_t k = 0; k < list_size(buffer_change->children) &&
                         STATUS_IS_SUCCESS(status); k++) {
                        status = bc_apply(list_get(buffer_change->children, k), buffer, redo);
                    }
                } else {
                    for (size_t k = list_size(buffer_change->children); 
                            k > 0 && STATUS_IS_SUCCESS(status); k--) {
                        status = bc_apply(list_get(buffer_change->children, k - 1), buffer, redo);
                    }
                }

                break;
            }
        default:
            {
                break;
            }
    }

    return status;
}

static Status bc_tc_apply(TextChange *text_change, Buffer *buffer, int redo)
{
    if ((redo && text_change->change_type == TCT_DELETE) ||
        (!redo && text_change->change_type == TCT_INSERT)) {

        char *str = malloc(text_change->str_len);

        if (str == NULL) {
            return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                "Unable to save deleted text");
        } 

        gb_get_range(buffer->data, text_change->pos.offset,
                str, text_change->str_len);
        text_change->str = str;

        RETURN_IF_FAIL(bf_set_bp(buffer, &text_change->pos));
        RETURN_IF_FAIL(bf_delete(buffer, text_change->str_len));
    } else if ((redo && text_change->change_type == TCT_INSERT) || 
               (!redo && text_change->change_type == TCT_DELETE)) {

        RETURN_IF_FAIL(bf_set_bp(buffer, &text_change->pos));
        RETURN_IF_FAIL(bf_insert_string(buffer, text_change->str, text_change->str_len, 1));

        free(text_change->str);
        text_change->str = NULL;
    }

    return STATUS_SUCCESS;
}

