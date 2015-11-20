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

#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <getopt.h>
#include "display.h"
#include "util.h"
#include "session.h"
#include "buffer.h"
#include "display.h"
#include "input.h"
#include "file.h"
#include "config.h"
#include "build_config.h"

static void print_usage(void);
static void print_version(void);
static int parse_args(int argc, char *argv[], int *file_args_index);

static void print_usage(void)
{
    const char *help_msg = 
"\n\
WED - Windows terminal EDitor\n\
\n\
Usage:\n\
wed [OPTIONS] [FILE]...\n\
\n\
OPTIONS:\n\
-h, --help          Print this message.\n\
-v, --version       Print version information.\n\
\n\
";

    printf("%s", help_msg);
}

static void print_version(void)
{
    const char *version = 
"\
WED - Windows terminal EDitor %s (Built %s)\n\
";

    printf(version, WED_VERSION, __DATE__ " " __TIME__);
}

static int parse_args(int argc, char *argv[], int *file_args_index)
{
    struct option wed_options[] = {
        { "help"   , no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    int c;
    /* Disable getopt printing an error message when encountering
     * an unrecognised option character, as we print our own
     * error message */
    opterr = 0;

    while ((c = getopt_long(argc, argv, "hv", wed_options, NULL)) != -1) {
        switch (c) {
            case 'h': 
                {
                    print_usage();
                    exit(0);
                }
            case 'v':
                {
                    print_version();
                    exit(0);
                }
            case '?': 
                {
                    fprintf(stderr, "Invalid option %s\n", argv[optind - 1]);
                    return 0;  
                }
            default: 
                {
                    fatal("Error parsing options");
                }
        }
    }

    /* Set the index in argv where file path arguments start */
    if (optind > 0) {
        *file_args_index = optind - 1;
    } else {
        *file_args_index = 0;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    int file_args_index;

    if (!parse_args(argc, argv, &file_args_index)) {
        return 1;
    }

    /* Use the locale specified by the environment */
    setlocale(LC_ALL, "");

    Session *sess = se_new();

    if (sess == NULL) {
        fatal("Out of memory - Unable to create Session");
    }

    /* Removed processed options */
    argc -= file_args_index;
    argv += file_args_index;

    if (!se_init(sess, argv, argc)) {
        fatal("Unable to initialise session");
    }

    ip_edit(sess);

    se_free(sess);
    
    return 0;
}
