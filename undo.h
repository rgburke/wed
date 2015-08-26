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

typedef enum {
    TCT_INSERT,
    TCT_DELETE
} TextChangeType;

typedef struct TextChange TextChange;

struct TextChange {
    TextChangeType change_type;
    BufferPos pos;
    size_t str_len;
    char *str;
};

typedef enum {
    BCT_TEXT_CHANGE,
    BCT_GROUPED_CHANGE
} BufferChangeType;

typedef union {
    TextChange *text_change;
} Change;

typedef struct BufferChange BufferChange;

struct BufferChange {
    BufferChangeType change_type;
    BufferChange *next;
    List *children;
    Change change;
};

typedef struct {
    BufferChange *undo;
    BufferChange *redo;
    int group_changes;
    int accept_new_changes;
} BufferChanges;

struct Buffer;

void bc_init(BufferChanges *);
void bc_free(BufferChanges *);
Status bc_add_text_insert(BufferChanges *, size_t, const BufferPos *);
Status bc_add_text_delete(BufferChanges *, const char *, size_t, const BufferPos *);
int bc_can_undo(const BufferChanges *);
int bc_can_redo(const BufferChanges *);
int bc_grouped_changes_started(const BufferChanges *);
Status bc_start_grouped_changes(BufferChanges *);
Status bc_end_grouped_changes(BufferChanges *);
Status bc_undo(BufferChanges *, struct Buffer *);
Status bc_redo(BufferChanges *, struct Buffer *);

#endif
