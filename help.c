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

#include <stdio.h>
#include <string.h>
#include "help.h"
#include "build_config.h"
#include "util.h"
#include "command.h"
#include "config.h"

typedef Status (*TableGenerator)(HelpTable *);
typedef void (*TableFree)(HelpTable *);

static void hp_free_help_table(HelpTable *);
static Status hp_bf_insert(Buffer *, const char *str);
static Status hp_insert_help_table(Buffer *, const HelpTable *);

Status hp_generate_help_text(Buffer *buffer)
{
    HelpTable help_table;
    Status status = STATUS_SUCCESS;

    RETURN_IF_FAIL(hp_bf_insert(buffer, 
            "\nWED - Windows terminal EDitor " WED_VERSION_LONG "\n"));

    static struct HelpSection {
        const char *title;
        struct HelpGenerator {
            TableGenerator table_generator;
            TableFree table_free;
        } help_generator;
    } const help_sections[] = {
        { "Default Key Bindings", { cm_generate_keybinding_table, NULL                   } },
        { "Config Variables"    , { cf_generate_variable_table  , cf_free_variable_table } },
        { "Commands"            , { cm_generate_command_table   , NULL                   } },
        { "Errors"              , { cm_generate_error_table     , cm_free_error_table    } }
    };

    const struct HelpSection *help_section;
    static const size_t help_section_num = ARRAY_SIZE(help_sections,
                                                      struct HelpSection);

    for (size_t k = 0; k < help_section_num; k++) {
        help_section = &help_sections[k];

        RETURN_IF_FAIL(hp_bf_insert(buffer, "\n"));
        RETURN_IF_FAIL(hp_bf_insert(buffer, help_section->title));
        RETURN_IF_FAIL(hp_bf_insert(buffer, "\n\n"));

        status = help_section->help_generator.table_generator(&help_table);

        if (!STATUS_IS_SUCCESS(status)) {
            hp_free_help_table(&help_table);
            return status;
        }

        status = hp_insert_help_table(buffer, &help_table);

        if (help_section->help_generator.table_free != NULL) {
            help_section->help_generator.table_free(&help_table);
        }

        hp_free_help_table(&help_table);
    }

    return status;
}

int hp_init_help_table(HelpTable *help_table, size_t rows, size_t cols)
{
    const size_t row_byte_num = rows * sizeof(const char **);
    const char ***table = help_table->table = malloc(row_byte_num);
    
    if (table == NULL) {
        return 0;
    }

    memset(table, 0, row_byte_num);

    help_table->rows = rows;
    help_table->cols = cols;

    for (size_t k = 0; k < rows; k++) {
        table[k] = malloc(cols * sizeof(const char *));
        
        if (table[k] == NULL) {
            hp_free_help_table(help_table);
            return 0;
        }
    }

    return 1;
}

static void hp_free_help_table(HelpTable *help_table)
{
    if (help_table == NULL || help_table->table == NULL) {
        return;
    }

    for (size_t k = 0; k < help_table->rows; k++) {
        free(help_table->table[k]);
    }

    free(help_table->table);

    memset(help_table, 0, sizeof(HelpTable));
}

static Status hp_bf_insert(Buffer *buffer, const char *str)
{
    return bf_insert_string(buffer, str, strlen(str), 1);
}

static Status hp_insert_help_table(Buffer *buffer, const HelpTable *help_table)
{
    if (help_table->table == NULL || help_table->rows < 1 ||
        help_table->cols < 1) {
        return STATUS_SUCCESS;
    }

    const char ***table = help_table->table;

    size_t max_col_widths[help_table->cols];
    memset(max_col_widths, 0, sizeof(size_t) * help_table->cols);
    size_t field_width;

    for (size_t row = 0; row < help_table->rows; row++) {
        for (size_t col = 0; col < help_table->cols; col++) {
            field_width = strlen(table[row][col]); 
            
            if (field_width > max_col_widths[col]) {
                max_col_widths[col] = field_width;
            }
        }
    }

    const size_t max_line_length = 1024;
    char line_buf[max_line_length];

    for (size_t col = 0; col < help_table->cols - 1; col++) {
        snprintf(line_buf, max_line_length, "%-*s | ",
                (int)max_col_widths[col], table[0][col]);
        RETURN_IF_FAIL(hp_bf_insert(buffer, line_buf));
    }
    
    snprintf(line_buf, max_line_length, "%s\n",
             table[0][help_table->cols - 1]);
    RETURN_IF_FAIL(hp_bf_insert(buffer, line_buf));

    size_t table_width = 3 * (help_table->cols - 1);

    for (size_t col = 0; col < help_table->cols; col++) {
        table_width += max_col_widths[col];
    }

    memset(line_buf, '-', sizeof(line_buf));
    table_width += 2;
    table_width = MIN(max_line_length - 2, table_width - 2);
    line_buf[table_width] = '\n';
    line_buf[table_width + 1] = '\0';

    size_t col_divider_pos = 0;

    for (size_t col = 0; col < help_table->cols - 1; col++) {
        col_divider_pos += max_col_widths[col] + 1;
        line_buf[col_divider_pos] = '|';
        col_divider_pos += 2;
    }

    RETURN_IF_FAIL(hp_bf_insert(buffer, line_buf));

    for (size_t row = 1; row < help_table->rows; row++) {
        for (size_t col = 0; col < help_table->cols - 1; col++) {
            snprintf(line_buf, max_line_length, "%-*s | ",
                    (int)max_col_widths[col], table[row][col]);
            RETURN_IF_FAIL(hp_bf_insert(buffer, line_buf));
        }

        snprintf(line_buf, max_line_length, "%s\n",
                 table[row][help_table->cols - 1]);
        RETURN_IF_FAIL(hp_bf_insert(buffer, line_buf));
    }

    return STATUS_SUCCESS;
}

