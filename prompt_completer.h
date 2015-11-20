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

#ifndef WED_PROMPT_COMPLETER_H
#define WED_PROMPT_COMPLETER_H

#include "prompt.h"
#include "session.h"
#include "list.h"

/* Rank suggestions so they can be ordered by rank when
 * presented to the user */
typedef enum {
    SR_EXACT_MATCH,
    SR_STARTS_WITH,
    SR_CONTAINS,
    SR_DEFAULT_MATCH, /* Used when any entry can match
                         e.g. completing a file path when only a directory
                         is specified displays all files in that directory */
    SR_NO_MATCH
} SuggestionRank;

/* File paths, buffer names, etc ... can be completed.
 * This structure holds each generated suggestion for completion */
typedef struct {
    char *text; /* Suggestion text */
    size_t text_len; /* Suggestion text length */
    SuggestionRank rank; /* Rank */
    const void *data; /* Data relevant to the suggestion */
} PromptSuggestion;

PromptSuggestion *pc_new_suggestion(const char *text, SuggestionRank,
                                    const void *data);
void pc_free_suggestion(PromptSuggestion *);
int pc_has_prompt_completer(PromptType);
int pc_show_suggestion_prompt(PromptType);
Status pc_run_prompt_completer(const Session *, Prompt *, int reverse);

#endif
