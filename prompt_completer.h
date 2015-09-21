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

typedef enum {
    SR_EXACT_MATCH,
    SR_STARTS_WITH,
    SR_CONTAINS,
    SR_DEFAULT_MATCH,
    SR_NO_MATCH
} SuggestionRank;

typedef struct {
    char *text;
    size_t text_len;
    SuggestionRank rank;
    const void *data;
} PromptSuggestion;

PromptSuggestion *pc_new_suggestion(const char *, SuggestionRank, const void *);
void pc_free_suggestion(PromptSuggestion *);
int pc_has_prompt_completer(PromptType);
int pc_show_suggestion_prompt(PromptType);
Status pc_run_prompt_completer(const Session *, Prompt *, int);

#endif
