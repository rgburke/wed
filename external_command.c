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

#include <unistd.h> 
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include "external_command.h"
#include "util.h"

#define SHELL "/bin/sh"

/* is: stdin, os: stdout, es: stderr */
Status ec_run_command(const char *cmd, InputStream *is, OutputStream *os,
                      OutputStream *es, int *cmd_status)
{
    /* Three Pipes:
     * in_pipe - parent writes to child's stdin
     * out_pipe - parent reads from child's stdout
     * err_pipe - parent reads from child's stderr */
    int in_pipe[2];
    int out_pipe[2];
    int err_pipe[2];

    errno = 0;

    if (pipe(in_pipe) == -1) {
        return st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                            "Unable to create pipe: %s", strerror(errno));
    }

    if (pipe(out_pipe) == -1) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                            "Unable to create pipe: %s", strerror(errno));
    }

    if (pipe(err_pipe) == -1) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                            "Unable to create pipe: %s", strerror(errno));
    }

    int child_in_fd = in_pipe[0];
    int child_out_fd = out_pipe[1];
    int child_err_out_fd = err_pipe[1];
    int parent_in_fd = out_pipe[0];
    int parent_out_fd = in_pipe[1];
    int parent_err_in_fd = err_pipe[0];

    pid_t pid = fork();

    if (pid == -1) {
        close(child_in_fd);
        close(child_out_fd);
        close(child_err_out_fd);
        close(parent_in_fd);
        close(parent_out_fd);
        close(parent_err_in_fd);
        return st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                            "Unable to fork: %s", strerror(errno));
    } else if (pid == 0) {
        /* Child process */

        int dup_success = dup2(child_in_fd, STDIN_FILENO) != -1 &&
                          dup2(child_out_fd, STDOUT_FILENO) != -1 &&
                          dup2(child_err_out_fd, STDERR_FILENO) != -1;

        close(child_in_fd);
        close(child_out_fd);
        close(child_err_out_fd);
        close(parent_in_fd);
        close(parent_out_fd);
        close(parent_err_in_fd);

        if (dup_success) {
            execl(SHELL, SHELL, "-c", cmd, NULL);
        }

        /* dup2 or execl failed */
        _exit(EXIT_FAILURE);
    }

    /* Parent process */
    Status status = STATUS_SUCCESS;

    close(child_in_fd);
    close(child_out_fd);
    close(child_err_out_fd);

    struct pollfd fds[] = {
        { .fd = parent_out_fd   , .events = POLLOUT },
        { .fd = parent_in_fd    , .events = POLLIN  },
        { .fd = parent_err_in_fd, .events = POLLIN  }
    };

    const nfds_t nfds = ARRAY_SIZE(fds, struct pollfd);

    int out_fd_flags = fcntl(parent_out_fd, F_GETFL);
    int in_fd_flags = fcntl(parent_in_fd, F_GETFL);
    int err_fd_flags = fcntl(parent_err_in_fd, F_GETFL);

    int fcntl_success = out_fd_flags != -1 &&
                        in_fd_flags != -1 &&
                        err_fd_flags != -1;

    if (!fcntl_success) {
        status = st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                              "fcntl F_GETFL failed: %s", strerror(errno));
        goto cleanup;
    }

    fcntl_success =
        fcntl(parent_out_fd, F_SETFL, out_fd_flags | O_NONBLOCK) != -1 &&
        fcntl(parent_in_fd, F_SETFL, in_fd_flags | O_NONBLOCK) != -1 &&
        fcntl(parent_err_in_fd, F_SETFL, err_fd_flags | O_NONBLOCK) != -1;

    if (!fcntl_success) {
        status = st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                              "fcntl F_SETFL failed: %s", strerror(errno));
        goto cleanup;
    }

    OutputStream *output_streams[] = { NULL, os, es };
    char in_buf[4096];
    char out_buf[4096];
    size_t in_bytes;
    size_t out_bytes;
    int pending_write = 0;

    if (is == NULL) {
        close(fds[0].fd);
        fds[0].fd = -1;
    }

    do {
        int poll_status = poll(fds, nfds, -1); 

        if (poll_status == -1) {
            if (errno == EINTR) {
                continue;
            }

            status = st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                                  "poll failed: %s", strerror(errno));
            break;
        }

        if (fds[0].fd != -1 && fds[0].revents & POLLOUT) {
            if (!pending_write) {
                status = is->read(is, in_buf, sizeof(in_buf), &in_bytes);
            }

            if (!STATUS_IS_SUCCESS(status)) {
                break;
            } else if (in_bytes == 0) {
                close(fds[0].fd);
                fds[0].fd = -1;
            } else {
                ssize_t written = write(fds[0].fd, in_buf, in_bytes);

                if (written == -1) {
                    if (errno != EAGAIN) {
                        status = st_get_error(
                                     ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                                     "Unable to write to child process "
                                     "stdin: %s", strerror(errno));
                        break;
                    } else {
                        pending_write = 1;
                    }
                } else {
                    pending_write = 0;
                }
            }
        }

        for (size_t k = 1; k < 3; k++) {
            if (fds[k].fd != -1 && fds[k].revents & (POLLIN | POLLHUP)) {
                ssize_t read_bytes = read(fds[k].fd, out_buf, sizeof(out_buf));

                if (read_bytes == -1) {
                    if (errno != EAGAIN) {
                        status = st_get_error(
                                     ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                                     "Unable to read child process output: %s",
                                     strerror(errno));
                        break;
                    }
                } else if (read_bytes == 0) {
                    close(fds[k].fd);
                    fds[k].fd = -1;
                } else if (output_streams[k] != NULL) {
                    status = output_streams[k]->write(output_streams[k],
                                                      out_buf,
                                                      read_bytes,
                                                      &out_bytes);

                    if (!STATUS_IS_SUCCESS(status)) {
                        break;
                    }
                }
            }
        }
    } while (!(fds[0].fd == -1 && fds[1].fd == -1 && fds[2].fd == -1));

cleanup:
    for (size_t k = 0; k < nfds; k++) {
        if (fds[k].fd != -1) {
            close(fds[k].fd);
        }
    }

    if (waitpid(pid, cmd_status, 0) == -1) {
        if (STATUS_IS_SUCCESS(status)) {
            status = st_get_error(ERR_UNABLE_TO_RUN_EXTERNAL_COMMAND,
                                  "Waiting for child process failed: %s",
                                  strerror(errno));
        }
    }

    return status;
}

int ec_cmd_successfull(int cmd_status)
{
    return WIFEXITED(cmd_status) && WEXITSTATUS(cmd_status) == 0;
}

