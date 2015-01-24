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

#include <stdlib.h>
#include "variable.h"
#include "util.h"
#include "status.h"

Status deep_copy_value(Value value, Value *new_val)
{
    *new_val = value;

    if (value.type != VAL_TYPE_STR || value.val.sval == NULL) {
        return STATUS_SUCCESS;
    }

    new_val->val.sval = strdupe(value.val.sval);

    if (new_val->val.sval == NULL) {
        return get_error(ERR_OUT_OF_MEMORY, "Out of memory - Unable to copy value");
    }

    return STATUS_SUCCESS;
}

void free_value(Value value)
{
    if (value.type != VAL_TYPE_STR || value.val.sval == NULL) {
        return;
    }

    free(value.val.sval);
}
