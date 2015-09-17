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

#include <string.h>
#include <assert.h>
#include "prompt_completer.h"
#include "util.h"

#define MAX_SUGGESTION_NUM 10

typedef Status (*PromptCompleter)(const Session *, List *, const char *, size_t);

static int pc_suggestion_comparator(const void *, const void *);
static Status pc_complete_buffer(const Session *, List *, const char *, size_t);

static const PromptCompleter pc_prompt_completers[PT_ENTRY_NUM] = {
    [PT_SAVE_FILE] = NULL,
    [PT_OPEN_FILE] = NULL,
    [PT_FIND]      = NULL,
    [PT_REPLACE]   = NULL,
    [PT_COMMAND]   = NULL,
    [PT_GOTO]      = NULL,
    [PT_BUFFER]    = pc_complete_buffer
};

PromptSuggestion *pc_new_suggestion(const char *text, SuggestionRank rank, const void *data)
{
    assert(!is_null_or_empty(text));

    PromptSuggestion *suggestion = malloc(sizeof(PromptSuggestion));
    RETURN_IF_NULL(suggestion);

    if ((suggestion->text = strdupe(text)) == NULL) {
        free(suggestion);
        return NULL;
    }

    suggestion->text_len = strlen(text);
    suggestion->rank = rank;
    suggestion->data = data;

    return suggestion;
}

void pc_free_suggestion(PromptSuggestion *suggestion)
{
    if (suggestion == NULL) {
        return;
    }

    free(suggestion->text);
    free(suggestion);
}

int pc_has_prompt_completer(PromptType prompt_type)
{
    assert(prompt_type < PT_ENTRY_NUM);
    return pc_prompt_completers[prompt_type] != NULL;
}

Status pc_run_prompt_completer(const Session *sess, Prompt *prompt)
{
    PromptType prompt_type = prompt->prompt_type;

    if (!pc_has_prompt_completer(prompt_type)) {
        return STATUS_SUCCESS;
    } 

    pr_clear_suggestions(prompt);

    char *prompt_content = pr_get_prompt_content(prompt);
    size_t prompt_content_len = strlen(prompt_content);

    if (prompt_content_len == 0) {
        free(prompt_content);
        return STATUS_SUCCESS;
    }

    PromptCompleter completer = pc_prompt_completers[prompt_type];
    Status status = completer(sess, prompt->suggestions, prompt_content, prompt_content_len);

    if (!STATUS_IS_SUCCESS(status)) {
        free(prompt_content);
        return status;
    }

    if (list_size(prompt->suggestions) == 0) {
        free(prompt_content);
        return STATUS_SUCCESS;
    }

    list_sort(prompt->suggestions, pc_suggestion_comparator);
    
    if (list_size(prompt->suggestions) > MAX_SUGGESTION_NUM) {

    }

    PromptSuggestion *inital_input = pc_new_suggestion(prompt_content,
                                                       SR_NO_MATCH, NULL);

    if (inital_input == NULL || !list_add(prompt->suggestions, inital_input)) {
        free(inital_input);
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to allocated suggestions");
    }

    status = pr_show_suggestion(prompt, 0);

    return status;
}

static int pc_suggestion_comparator(const void *s1, const void *s2)
{
    const PromptSuggestion *suggestion1 = *(const PromptSuggestion **)s1;
    const PromptSuggestion *suggestion2 = *(const PromptSuggestion **)s2;

    if (suggestion1->rank < suggestion2->rank) {
        return -1;
    }

    return suggestion1->rank - suggestion2->rank;
}

static Status pc_complete_buffer(const Session *sess, List *suggestions,
                                 const char *str, size_t str_len)
{
    const Buffer *buffer = sess->buffers;
    const FileInfo *file_info;
    PromptSuggestion *suggestion;
    SuggestionRank rank;

    while (buffer != NULL) {
        file_info = &buffer->file_info;
        rank = SR_NO_MATCH;

        if (strcmp(file_info->rel_path, str) == 0) {
            rank = SR_EXACT_MATCH;
        } else if (strncmp(file_info->rel_path, str, str_len) == 0) {
            rank = SR_STARTS_WITH;
        } else if (strstr(file_info->rel_path, str) != NULL) {
            rank = SR_CONTAINS;
        }

        if (rank != SR_NO_MATCH) {
            suggestion = pc_new_suggestion(file_info->rel_path, rank, buffer);
            
            if (suggestion == NULL || !list_add(suggestions, suggestion)) {
                free(suggestion);
                return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                    "Unable to allocated suggested buffer");
            }
        }

        buffer = buffer->next;
    }

    return STATUS_SUCCESS;
}
