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

#ifndef WED_EXTERNAL_COMMAND_H
#define WED_EXTERNAL_COMMAND_H

#include "status.h"

typedef struct InputStream InputStream;

struct InputStream {
    Status (*read)(InputStream *is, char buf[], size_t buf_len,
                   size_t *bytes_read);
    Status (*close)(InputStream *is);
};

typedef struct OutputStream OutputStream;

struct OutputStream {
    Status (*write)(OutputStream *os, const char buf[], size_t buf_len,
                    size_t *bytes_written);
    Status (*close)(OutputStream *os);
};

Status ec_run_command(const char *cmd, InputStream *is, OutputStream *os,
                      OutputStream *es, int *cmd_status);
int ec_cmd_successfull(int cmd_status);

#endif
