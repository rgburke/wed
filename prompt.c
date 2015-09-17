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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "prompt.h"
#include "session.h"
#include "util.h"
#include "prompt_completer.h"

static const char *pr_get_suggestion_prompt_text(const Prompt *);

Prompt *pr_new(Buffer *prompt_buffer)
{
    assert(prompt_buffer != NULL);

    Prompt *prompt = malloc(sizeof(Prompt));
    RETURN_IF_NULL(prompt);

    memset(prompt, 0, sizeof(Prompt));

    if ((prompt->suggestions = list_new()) == NULL) {
        free(prompt);
        return NULL;
    }

    prompt->prompt_buffer = prompt_buffer;

    return prompt;
}

void pr_free(Prompt *prompt, int free_prompt_buffer)
{
    if (prompt == NULL) {
        return;
    }

    if (free_prompt_buffer) {
        bf_free(prompt->prompt_buffer);
    }

    list_free_all(prompt->suggestions);
    free(prompt->prompt_text);
    free(prompt);
}

Status pr_reset_prompt(Prompt *prompt, PromptType prompt_type, 
                       const char *prompt_text, List *history,
                       int show_last_cmd)
{
    RETURN_IF_FAIL(pr_set_prompt_text(prompt, prompt_text));

    prompt->prompt_type = prompt_type;
    prompt->cancelled = 0;
    prompt->history = history;
    pr_clear_suggestions(prompt);

    const char *prompt_content = "";

    if (history != NULL) {
        prompt->history_index = list_size(history);

        if (show_last_cmd && prompt->history_index > 0) {
            prompt_content = list_get(history, --prompt->history_index); 
        }
    }

    RETURN_IF_FAIL(bf_set_text(prompt->prompt_buffer, prompt_content));

    return bf_select_all_text(prompt->prompt_buffer);
}

Status pr_set_prompt_text(Prompt *prompt, const char *prompt_text)
{
    assert(!is_null_or_empty(prompt_text));

    if (prompt->prompt_text != NULL) {
        free(prompt->prompt_text);
    }

    prompt->prompt_text = strdupe(prompt_text);
    
    if (prompt_text != NULL && prompt->prompt_text == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out of memory - " 
                            "Unable to set prompt text");
    }

    return STATUS_SUCCESS;
}

Buffer *pr_get_prompt_buffer(const Prompt *prompt)
{
    return prompt->prompt_buffer;
}

PromptType pr_get_prompt_type(const Prompt *prompt)
{
    return prompt->prompt_type;
}

const char *pr_get_prompt_text(const Prompt *prompt)
{
    size_t suggestion_num = list_size(prompt->suggestions);

    if (prompt->show_suggestion_prompt &&
        suggestion_num > 1 &&
        prompt->suggestion_index != suggestion_num - 1) {
        return pr_get_suggestion_prompt_text(prompt);
    }

    static char suggestion_prompt_text[MAX_CMD_PROMPT_LENGTH + 1];
    snprintf(suggestion_prompt_text, MAX_CMD_PROMPT_LENGTH + 1, 
             "%s", prompt->prompt_text);

    return suggestion_prompt_text;
}

static const char *pr_get_suggestion_prompt_text(const Prompt *prompt)
{
    size_t suggestion_num = list_size(prompt->suggestions) - 1;

    static char suggestion_prompt_text[MAX_CMD_PROMPT_LENGTH + 1];
    snprintf(suggestion_prompt_text, MAX_CMD_PROMPT_LENGTH + 1, "%s (%zu of %zu)",
             prompt->prompt_text, prompt->suggestion_index + 1, suggestion_num);

    return suggestion_prompt_text;
}

char *pr_get_prompt_content(const Prompt *prompt)
{
    return bf_to_string(prompt->prompt_buffer);
}

int pr_prompt_cancelled(const Prompt *prompt)
{
    return prompt->cancelled;
}

void pr_prompt_set_cancelled(Prompt *prompt, int cancelled)
{
    prompt->cancelled = cancelled;
}

void pr_show_suggestion_prompt(Prompt *prompt)
{
    prompt->show_suggestion_prompt = 1;
}

void pr_hide_suggestion_prompt(Prompt *prompt)
{
    prompt->show_suggestion_prompt = 0;
}

Status pr_previous_entry(Prompt *prompt)
{
    if (prompt->history == NULL) {
        return STATUS_SUCCESS;
    }

    if (prompt->history_index > 0) {
        const char *prompt_content = list_get(prompt->history, 
                                              --prompt->history_index);
        return bf_set_text(prompt->prompt_buffer, prompt_content);
    }

    return STATUS_SUCCESS;
}

Status pr_next_entry(Prompt *prompt)
{
    if (prompt->history == NULL) {
        return STATUS_SUCCESS;
    }

    size_t entries = list_size(prompt->history);

    if (prompt->history_index < entries) {
        const char *prompt_content;

        if (++prompt->history_index == entries) {
            prompt_content = "";
        } else {
            prompt_content = list_get(prompt->history, prompt->history_index);
        }

        return bf_set_text(prompt->prompt_buffer, prompt_content);
    }

    return STATUS_SUCCESS;
}

void pr_clear_suggestions(Prompt *prompt)
{
    prompt->suggestion_index = 0;
    list_free_values_custom(prompt->suggestions, (ListEntryFree)pc_free_suggestion);
    list_clear(prompt->suggestions);
}

Status pr_show_next_suggestion(Prompt *prompt)
{
    size_t suggestion_num = list_size(prompt->suggestions);

    if (suggestion_num == 0) {
        return STATUS_SUCCESS;
    }

    size_t suggestion_index = prompt->suggestion_index + 1;
    suggestion_index %= suggestion_num;
    
    return pr_show_suggestion(prompt, suggestion_index);
}

Status pr_show_suggestion(Prompt *prompt, size_t suggestion_index)
{
    size_t suggestion_num = list_size(prompt->suggestions);
    assert(suggestion_index < suggestion_num);

    if (!(suggestion_index < suggestion_num)) {
        return STATUS_SUCCESS;
    }
    
    const PromptSuggestion *suggestion = list_get(prompt->suggestions, suggestion_index);
    RETURN_IF_FAIL(bf_set_text(prompt->prompt_buffer, suggestion->text));
    prompt->suggestion_index = suggestion_index;

    return STATUS_SUCCESS;
}
