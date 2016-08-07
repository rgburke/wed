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
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <getopt.h>
#include "wed.h"
#include "util.h"
#include "session.h"
#include "buffer.h"
#include "input.h"
#include "file.h"
#include "config.h"
#include "build_config.h"

static void we_init_wedopt(WedOpt *);
static void we_free_wedopt(WedOpt *);
static void we_print_usage(void);
static void we_print_version(void);
static int we_parse_args(WedOpt *wed_opt, int argc, char *argv[],
                         int *file_args_index);

static void we_init_wedopt(WedOpt *wed_opt)
{
    memset(wed_opt, 0, sizeof(WedOpt));
}

static void we_free_wedopt(WedOpt *wed_opt)
{
    free(wed_opt->keystr_input);
    free(wed_opt->config_file_path);
}

static void we_print_usage(void)
{
    const char *help_msg = 
"\n\
WED - Windows terminal EDitor\n\
\n\
Usage:\n\
wed [OPTIONS] [FILE]...\n\
\n\
OPTIONS:\n\
-c, --config-file WEDRC    Load the WEDRC config file after all other\n\
                           config files have been processed.\n\
-h, --help                 Print this message and exit.\n\
-k, --key-string KEYSTR    Process KEYSTR string representation of key\n\
                           presses after initialisation.\n\
-v, --version              Print version information and exit.\n\
\n\
";

    printf("%s", help_msg);
}

static void we_print_version(void)
{
    const char *version = 
"\
WED - Windows terminal EDitor %s (%s, Built %s)\n\
";

    printf(version, WED_VERSION, WED_VERSION_LONG, WED_BUILD_DATETIME);

    struct Feature {
        const char *name;
        const int enabled;
    } const features[] = {
        { "Lua", WED_FEATURE_LUA },
        { "GNU Source-highlight", WED_FEATURE_GNU_SOURCE_HIGHLIGHT }
    };

    const size_t feature_num = ARRAY_SIZE(features, struct Feature);

    printf("Features:\n");

    for (size_t k = 0; k < feature_num; k++) {
        printf("%c %s\n", features[k].enabled ? '+' : '-', features[k].name);
    }
}

static int we_parse_args(WedOpt *wed_opt, int argc, char *argv[],
                         int *file_args_index)
{
    struct option wed_options[] = {
        { "config-file", required_argument, 0, 'c' },
        { "help"       , no_argument      , 0, 'h' },
        { "key-string" , required_argument, 0, 'k' },
        { "version"    , no_argument      , 0, 'v' },
        /* Used only for running tests by run_text_tests.sh
         * so don't mention in help text above */
        { "test-mode"  , no_argument      , 0,  0  },
        { 0, 0, 0, 0 }
    };

    int c;
    /* Disable getopt printing an error message when encountering
     * an unrecognised option character, as we print our own
     * error message */
    opterr = 0;


    while ((c = getopt_long(argc, argv, ":hvc:k:", wed_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                {
                    if ((wed_opt->config_file_path = strdup(optarg)) == NULL) {
                        fatal("Out Of Memory - Unable to parse options");
                    }

                    break;
                }
            case 'h': 
                {
                    we_print_usage();
                    exit(0);
                }
            case 'k':
                {
                    if ((wed_opt->keystr_input = strdup(optarg)) == NULL) {
                        fatal("Out Of Memory - Unable to parse options");
                    }

                    break;
                }
            case 'v':
                {
                    we_print_version();
                    exit(0);
                }
            case 0:
                {
                    wed_opt->test_mode = 1;
                    break;
                }
            case ':':
                {
                    switch (optopt) {
                        case 'c':
                            {
                                fprintf(stderr, "Option -c, --config-file "
                                        "requires a WEDRC filepath argument\n");
                                break;

                            }
                        case 'k':
                            {
                                fprintf(stderr, "Option -k, --key-string "
                                        "requires a KEYSTR argument\n");
                                break;
                            }
                        default:
                            {
                                fprintf(stderr, "Unknown option: %c\n",
                                        optopt);
                                break;
                            }
                    }

                    return 0;
                }
            case '?': 
                {
                    fprintf(stderr, "Invalid option: %s\n", argv[optind - 1]);
                    return 0;  
                }
            default: 
                {
                    fatal("Error parsing options");
                }
        }
    }

    if (wed_opt->test_mode && wed_opt->keystr_input == NULL) {
        fatal("KEYSTR input must be specified in test mode");
    }

    /* Set the index in argv where file path arguments start */
    if (optind > 0) {
        *file_args_index = optind;
    } else {
        *file_args_index = 1;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    int file_args_index;
    WedOpt wed_opt;

    we_init_wedopt(&wed_opt);

    if (!we_parse_args(&wed_opt, argc, argv, &file_args_index)) {
        return 1;
    }

    /* Use the locale specified by the environment */
    setlocale(LC_ALL, "");

    Session *sess = se_new();

    if (sess == NULL) {
        fatal("Out Of Memory - Unable to create Session");
    }

    /* Removed processed options */
    argc -= file_args_index;
    argv += file_args_index;

    if (!se_init(sess, &wed_opt, argv, argc)) {
        fatal("Unable to initialise session");
    }

    ip_edit(sess);

    int return_code = 0;

    if (wed_opt.test_mode) {
        return_code = se_has_errors(sess);    
    }

    se_free(sess);

    we_free_wedopt(&wed_opt);
    
    return return_code;
}
