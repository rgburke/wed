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

#ifndef WED_PROMPT_H
#define WED_PROMPT_H

#include "buffer.h"
#include "list.h"

typedef enum {
    PT_SAVE_FILE,
    PT_OPEN_FILE,
    PT_FIND,
    PT_REPLACE,
    PT_COMMAND
} PromptType;

typedef struct {
    Buffer *prompt_buffer; /* Used for command input e.g. Find & Replace, file name input, etc... */
    char *prompt_text; /* Command instruction/description */
    int cancelled; /* Did the user quit the last prompt */
    List *history; /* Stores previous user entries */
    size_t history_index; /* Index of previous entry shown */
    PromptType prompt_type;
} Prompt; 

Prompt *pr_new(Buffer *);
void pr_free(Prompt *, int);
Status pr_reset_prompt(Prompt *, PromptType, const char *, List *, int);
Status pr_set_prompt_text(Prompt *, const char *);
Buffer *pr_get_prompt_buffer(const Prompt *);
PromptType pr_get_prompt_type(const Prompt *);
const char *pr_get_prompt_text(const Prompt *);
char *pr_get_prompt_content(const Prompt *);
int pr_prompt_cancelled(const Prompt *);
void pr_prompt_set_cancelled(Prompt *, int);
Status pr_previous_entry(Prompt *);
Status pr_next_entry(Prompt *);

#endif
