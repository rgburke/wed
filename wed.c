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

#include <unistd.h>
#include <locale.h>
#include "display.h"
#include "util.h"
#include "session.h"
#include "buffer.h"
#include "display.h"
#include "input.h"
#include "file.h"

int parse_args(int argc, char *argv[], Session *sess)
{
    /* TODO Write this function. Probably using getopt */
    
    /*To stop compiler warnings */
    argc++;
    (void)argv;
    (void)sess;

    return 1;
}

/* TODO This function needs to generate errors or return a boolean.
 * Also maybe this should be moved to session.c */
void init_session(char *buffers[], int buffer_num, Session *sess)
{
    FileInfo file_info;

    /* Limited to one file for the moment */
    buffer_num = 2;

    for (int k = 1; k < buffer_num; k++) {
        init_fileinfo(&file_info, buffers[k]);

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
        return;
    }
}

int main(int argc, char *argv[])
{
    Session *sess = new_session();

    if (!parse_args(argc, argv, sess)) {
        return 1;
    }

    setlocale(LC_ALL, "");
    init_session(argv, argc, sess);

    edit(sess);

    free_session(sess);
    
    return 0;
}
