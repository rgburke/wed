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

#ifndef WED_UNDO_H
#define WED_UNDO_H

#include <stddef.h>
#include "list.h"
#include "status.h"
#include "buffer_pos.h"

/* An unlimited linear undo/redo implementation */

/* All operations on the text in a buffer can be thought of
 * as a sequence of insertions and deletions. That is, there are two
 * base operations that are performed on a buffer from which
 * all higher level operations derive. We can therefore
 * track all changes to buffer text by tracking the insertions and deletions
 * that take place and grouping them appropriately. */

/* The enum below is used to categorise a text change
 * as either an insert or a delete */
typedef enum {
    TCT_INSERT,
    TCT_DELETE
} TextChangeType;

typedef struct TextChange TextChange;

/* The properties of a text change
 * i.e. The properties of an insert or delete */
struct TextChange {
    TextChangeType change_type; /* Insert or delete */
    BufferPos pos; /* The position in the buffer where this change took place */
    size_t str_len; /* The length of the text inserted or deleted */
    char *str; /* For delete: the text from the buffer that has been deleted
                  For insert: this variable is NULL
                  str is NULL for insert because the buffer already stores
                  the text that has been inserted, so storing it here also
                  would be unnecessary duplication */
};

/* Text changes aren't the only possible changes that we could want
 * to track to provide undo/redo e.g. A user closes a buffer
 * accidentally and re-opens it with <C-z>
 * The enum and union below are to allow other types of change to be
 * tracked whilst adding extra functionality such as the ability to
 * group changes together into a single action.
 * Currently only text changes are tracked and have undo/redo */

/* The type of change that took place on the buffer */
typedef enum {
    BCT_TEXT_CHANGE, /* A text change */
    BCT_GROUPED_CHANGE /* A change comprised of multiple child changes
                          i.e. multiple changes grouped together into one */
} BufferChangeType;

/* Abstraction of a change to allow other types of changes
 * to be tracked */
typedef union {
    TextChange *text_change;
} Change;

typedef struct BufferChange BufferChange;

/* A change to a buffer */
struct BufferChange {
    BufferChangeType change_type; /* The type of change */
    BufferChange *next; /* Changes are stored in a linked list */
    List *children; /* If this is not a grouped change then 
                       children will be NULL, otherwise this is a list
                       of all the child BufferChange changes this 
                       grouped change is composed of */
    Change change; /* The actual change details and data. For a grouped
                      change this is NULL (i.e. the pointers in the union
                      are NULL) as this change is simply a container for
                      its child changes */
    size_t version; /* Set to 0 and incremented when a sequential change is
                       grouped to this change. This is used by the
                       BufferChangeState struct to determine if a buffer has
                       been modified */
};

/* This is the top level struct containing undo and redo stacks
 * that track all changes made to a buffer */
typedef struct {
    BufferChange *undo; /* Undo stack */
    BufferChange *redo; /* Redo stack */
    int group_changes; /* When true all subsequent BufferChange's are
                          grouped together as children of a single
                          BufferChange until set to false */
    int accept_new_changes; /* Set true by default. When false all further
                               changes are ignored. This is used when
                               actually applying an undo/redo which
                               will require inserting/deleting text */
} BufferChanges;

/* Stores the most recent change on the undo stack. This can be used to take
 * a snapshot and determine in future if a subsequent change has been made to
 * a buffer */
typedef struct {
    const BufferChange *change; /* This is used only for comparison
                                   with the most recent change on the
                                   undo stack and is never dereferenced
                                   as it could point to freed memory */
    size_t version; /* The version of the most recent change on the undo
                       stack */
} BufferChangeState;

struct Buffer;

void bc_init(BufferChanges *);
void bc_free(BufferChanges *);
Status bc_add_text_insert(BufferChanges *, size_t str_len,const BufferPos *);
Status bc_add_text_delete(BufferChanges *, const char *str, size_t str_len,
                          const BufferPos *);
int bc_can_undo(const BufferChanges *);
int bc_can_redo(const BufferChanges *);
int bc_grouped_changes_started(const BufferChanges *);
Status bc_start_grouped_changes(BufferChanges *);
Status bc_end_grouped_changes(BufferChanges *);
Status bc_undo(BufferChanges *, struct Buffer *);
Status bc_redo(BufferChanges *, struct Buffer *);
BufferChangeState bc_get_current_state(const BufferChanges *);
int bc_has_state_changed(const BufferChanges *, BufferChangeState);

#endif
