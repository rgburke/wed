/*
 * Copyright (C) 2016 Richard Burke
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
#include "clipboard.h"
#include "external_command.h"

#define CLIPBOARD_CMD "wed-clipboard"
#define CLIPBOARD_CMD_USABLE CLIPBOARD_CMD " --usable"
#define CLIPBOARD_CMD_COPY   CLIPBOARD_CMD " --copy"
#define CLIPBOARD_CMD_PASTE  CLIPBOARD_CMD " --paste"

void cl_init(Clipboard *clipboard)
{
    memset(clipboard, 0, sizeof(Clipboard));

    int cmd_status;
    Status status = ec_run_command(CLIPBOARD_CMD_USABLE, NULL, NULL, NULL,
                                   &cmd_status);

    if (STATUS_IS_SUCCESS(status) && ec_cmd_successfull(cmd_status)) {
        clipboard->type = CT_EXTERNAL;
    } else {
        clipboard->type = CT_INTERNAL;
    }
}

void cl_free(Clipboard *clipboard)
{
    if (clipboard->type == CT_INTERNAL) {
        bf_free_textselection(&clipboard->text_selection);
    }
}

/* TODO use function pointers rather than if-else chain
 * Also add support for multiple internal clipboards - can be unlimited if
 * we allow arbitrary names */
Status cl_copy(Clipboard *clipboard, Buffer *buffer)
{
    Status status = STATUS_SUCCESS;
    Range range;

    if (!bf_get_range(buffer, &range)) {
        return status;
    }

    if (clipboard->type == CT_INTERNAL) {
        status = bf_copy_selected_text(buffer, &clipboard->text_selection);
    } else if (clipboard->type == CT_EXTERNAL) {
        BufferInputStream bis;
        RETURN_IF_FAIL(bf_get_buffer_input_stream(&bis, buffer, &range));
        
        int cmd_status;
        status = ec_run_command(CLIPBOARD_CMD_COPY, (InputStream *)&bis,
                                NULL, NULL, &cmd_status);

        bis.is.close((InputStream *)&bis);
        RETURN_IF_FAIL(status);

        if (!ec_cmd_successfull(cmd_status)) {
            status = st_get_error(ERR_CLIPBOARD_ERROR,
                                  "Unable to copy to system clipboard");
        }
    }

    return status;
}

Status cl_paste(Clipboard *clipboard, Buffer *buffer)
{
    Status status = STATUS_SUCCESS;

    if (clipboard->type == CT_INTERNAL) {
        if (clipboard->text_selection.str != NULL) {
            status = bf_insert_textselection(buffer,
                                             &clipboard->text_selection, 1);
        }
    } else if (clipboard->type == CT_EXTERNAL) {
        BufferOutputStream bos;
        RETURN_IF_FAIL(
            bf_get_buffer_output_stream(&bos, buffer, &buffer->pos, 0)
        ); 
        Status status = bc_start_grouped_changes(&buffer->changes);
        int cmd_status;

        if (STATUS_IS_SUCCESS(status)) {
            status = ec_run_command(CLIPBOARD_CMD_PASTE, NULL,
                                    (OutputStream *)&bos, NULL, &cmd_status);
        }

        bos.os.close((OutputStream *)&bos);
        bc_end_grouped_changes(&buffer->changes);
        RETURN_IF_FAIL(status);

        if (!ec_cmd_successfull(cmd_status)) {
            status = st_get_error(ERR_CLIPBOARD_ERROR,
                                  "Unable to paste from system clipboard");
        } else {
            bp_advance_to_offset(&buffer->pos, bos.write_pos.offset);
        }
    }

    return status;
}

/* Cut is not really a direct clipboard action, but exposing the functionality
 * in this way is convenient and consistent with the functions above.
 * Whether the clipboard entity should be tied so closely to the buffer entity
 * is another question worth considering i.e. what if we don't want to
 * copy/paste from/to a buffer? Should {Input,Output}Streams be passed
 * instead? */
Status cl_cut(Clipboard *clipboard, Buffer *buffer)
{
    Status status;
    Range range;

    if (!bf_get_range(buffer, &range)) {
        return status;
    }

    if (clipboard->type == CT_INTERNAL) {
        status = bf_cut_selected_text(buffer, &clipboard->text_selection);
    } else if (clipboard->type == CT_EXTERNAL) {
        status = cl_copy(clipboard, buffer);

        if (STATUS_IS_SUCCESS(status)) {
            status = bf_delete_range(buffer, &range); 
        }
    }

    return status;
}

