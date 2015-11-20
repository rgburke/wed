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

/* Max length of prompt text */
#define MAX_CMD_PROMPT_LENGTH 50

typedef enum {
    PT_SAVE_FILE,
    PT_OPEN_FILE,
    PT_FIND,
    PT_REPLACE,
    PT_COMMAND,
    PT_GOTO,
    PT_BUFFER,
    PT_ENTRY_NUM
} PromptType;

/* Structure for controlling prompt */
typedef struct {
    Buffer *prompt_buffer; /* Used for prompt input e.g. Find & Replace,
                              file name input, etc... */
    char *prompt_text; /* Prompt message presented to user
                          e.g. Find, Replace, Save As, ... */
    int cancelled; /* Did the user quit the last prompt */
    List *history; /* Stores previous user entries */
    size_t history_index; /* Index of previous entry shown */
    PromptType prompt_type; /* This is used to drive prompt completion */
    List *suggestions; /* Stores suggested prompt completions
                          e.g. completed file path, buffer name */
    size_t suggestion_index; /* Currently displayed suggestion */
    int show_suggestion_prompt; /* True if suggestions should be shown */
} Prompt; 

Prompt *pr_new(Buffer *prompt_buffer);
void pr_free(Prompt *, int free_prompt_buffer);
Status pr_reset_prompt(Prompt *, PromptType, const char *prompt_text,
                       List *history, int show_last_cmd);
Status pr_set_prompt_text(Prompt *, const char *prompt_text);
Buffer *pr_get_prompt_buffer(const Prompt *);
PromptType pr_get_prompt_type(const Prompt *);
const char *pr_get_prompt_text(const Prompt *);
char *pr_get_prompt_content(const Prompt *);
int pr_prompt_cancelled(const Prompt *);
void pr_prompt_set_cancelled(Prompt *, int cancelled);
void pr_show_suggestion_prompt(Prompt *);
void pr_hide_suggestion_prompt(Prompt *);
Status pr_previous_entry(Prompt *);
Status pr_next_entry(Prompt *);
size_t pr_suggestion_num(const Prompt *);
void pr_clear_suggestions(Prompt *);
Status pr_show_next_suggestion(Prompt *);
Status pr_show_previous_suggestion(Prompt *);
Status pr_show_suggestion(Prompt *, size_t suggestion_index);

#endif
