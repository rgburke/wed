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

/* For DT_DIR */
#define _BSD_SOURCE

/* In case user invokes completion on a directory 
 * containing many files */
#define MAX_DIR_ENT_NUM 1000

#include <stdio.h> 
#include <dirent.h> 
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "prompt_completer.h"
#include "util.h"

typedef Status (*PromptCompleter)(const Session *, List *, const char *, size_t);

typedef struct {
    PromptCompleter prompt_completer;
    int show_suggestion_prompt;
} PromptCompleterConfig;

static int pc_suggestion_comparator(const void *, const void *);
static Status pc_complete_buffer(const Session *, List *, const char *, size_t);
static Status pc_complete_path(const Session *, List *, const char *, size_t);

static const PromptCompleterConfig pc_prompt_completers[PT_ENTRY_NUM] = {
    [PT_SAVE_FILE] = { pc_complete_path  , 0 },
    [PT_OPEN_FILE] = { pc_complete_path  , 0 },
    [PT_FIND]      = { NULL              , 0 },
    [PT_REPLACE]   = { NULL              , 0 },
    [PT_COMMAND]   = { NULL              , 0 },
    [PT_GOTO]      = { NULL              , 0 },
    [PT_BUFFER]    = { pc_complete_buffer, 1 }
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
    return pc_prompt_completers[prompt_type].prompt_completer != NULL;
}

int pc_show_suggestion_prompt(PromptType prompt_type)
{
    assert(prompt_type < PT_ENTRY_NUM);
    return pc_has_prompt_completer(prompt_type) &&
           pc_prompt_completers[prompt_type].show_suggestion_prompt; 
}

Status pc_run_prompt_completer(const Session *sess, Prompt *prompt, int reverse)
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

    const PromptCompleterConfig *pcc = &pc_prompt_completers[prompt_type];
    PromptCompleter completer = pcc->prompt_completer;
    Status status = completer(sess, prompt->suggestions, prompt_content, prompt_content_len);

    if (!STATUS_IS_SUCCESS(status)) {
        free(prompt_content);
        return status;
    }

    if (pr_suggestion_num(prompt) == 0) {
        free(prompt_content);
        return STATUS_SUCCESS;
    }

    list_sort(prompt->suggestions, pc_suggestion_comparator);
    
    PromptSuggestion *inital_input = pc_new_suggestion(prompt_content,
                                                       SR_NO_MATCH, NULL);

    if (inital_input == NULL || !list_add(prompt->suggestions, inital_input)) {
        free(inital_input);
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to allocated suggestions");
    }

    assert(pr_suggestion_num(prompt) > 1);

    size_t start_index = 0;

    if (reverse) {
        start_index = pr_suggestion_num(prompt) - 2;
    }

    status = pr_show_suggestion(prompt, start_index);

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
    const char *buffer_path;
    PromptSuggestion *suggestion;
    SuggestionRank rank;

    while (buffer != NULL) {
        rank = SR_NO_MATCH;

        if (fi_has_file_path(&buffer->file_info)) {
            buffer_path = buffer->file_info.rel_path;
        } else {
            buffer_path = buffer->file_info.file_name;
        }

        if (strcmp(buffer_path, str) == 0) {
            rank = SR_EXACT_MATCH;
        } else if (strncmp(buffer_path, str, str_len) == 0) {
            rank = SR_STARTS_WITH;
        } else if (strstr(buffer_path, str) != NULL) {
            rank = SR_CONTAINS;
        }

        if (rank != SR_NO_MATCH) {
            suggestion = pc_new_suggestion(buffer_path, rank, buffer);
            
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

static Status pc_complete_path(const Session *sess, List *suggestions,
                               const char *str, size_t str_len)
{
    (void)sess;

    if (strcmp("~", str) == 0) {
        str = getenv("HOME"); 
        str_len = strlen(str); 
    }

    char *path1 = strdupe(str); 
    char *path2 = strdupe(str);

    if (path1 == NULL || path2 == NULL) {
        return st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                            "Unable to allocate memory for path");
    }

    Status status = STATUS_SUCCESS;

    const char *dir_path;
    const char *file_name;
    size_t file_name_len;
    int is_root_only = (strcmp("/", path1) == 0);

    if (!is_root_only &&
        path1[str_len - 1] == '/') {

        path1[str_len - 1] = '\0';
        dir_path = path1;
        free(path2);
        path2 = NULL;
        file_name = NULL;
        file_name_len = 0;
    } else {
        dir_path = dirname(path1);
        
        if (is_root_only) {
            file_name = NULL;
            file_name_len = 0;
        } else {
            file_name = basename(path2);
            file_name_len = strlen(file_name);
        }
    }

    int home_dir_path = (dir_path[0] == '~');
    const char *canon_dir_path;
    DIR *dir = NULL;

    if (home_dir_path) {
        const char *home_path = getenv("HOME"); 
        canon_dir_path = concat(home_path, dir_path + 1);

        if (canon_dir_path == NULL) {
            status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                 "Unable to allocated path");
            goto cleanup;
        }
    } else {
        canon_dir_path = dir_path;
    }

    dir = opendir(canon_dir_path);

    if (home_dir_path) {
        free((char *)canon_dir_path);
    }

    if (dir == NULL) {
        goto cleanup;
    }
    
    if (strcmp(dir_path, "/") == 0) {
        dir_path = "";
    }

    struct dirent *dir_ent;
    PromptSuggestion *suggestion;
    SuggestionRank rank;
    size_t dir_ent_num = 0;
    errno = 0;

    while (dir_ent_num++ < MAX_DIR_ENT_NUM &&
           (dir_ent = readdir(dir)) != NULL) {
        if (errno) {
            status = st_get_error(ERR_UNABLE_TO_READ_DIRECTORY,
                                  "Unable to read from directory - %s",
                                  strerror(errno));
            goto cleanup;
        }

        if (strcmp(".", dir_ent->d_name) == 0 ||
            strcmp("..", dir_ent->d_name) == 0) {
            continue;
        }

        rank = SR_NO_MATCH;

        if (file_name == NULL) {
            rank = SR_DEFAULT_MATCH;            
        } else if (strcmp(file_name, dir_ent->d_name) == 0) {
            rank = SR_EXACT_MATCH;
        } else if (strncmp(file_name, dir_ent->d_name, file_name_len) == 0) {
            rank = SR_STARTS_WITH; 
        } else if (strstr(file_name, dir_ent->d_name) != NULL) {
            rank = SR_CONTAINS;
        }

        if (rank != SR_NO_MATCH) {
            char *suggestion_path;

            if (dir_ent->d_type == DT_DIR) {
                suggestion_path = concat_all(4, dir_path, "/", dir_ent->d_name, "/");
            } else {
                suggestion_path = concat_all(3, dir_path, "/", dir_ent->d_name);
            }

            if (suggestion_path == NULL) {
                status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                      "Unable to allocated suggested path");
                goto cleanup;
            }

            suggestion = pc_new_suggestion(suggestion_path, rank, NULL);

            free(suggestion_path);
            
            if (suggestion == NULL || !list_add(suggestions, suggestion)) {
                free(suggestion);
                status = st_get_error(ERR_OUT_OF_MEMORY, "Out Of Memory - "
                                      "Unable to allocated suggested buffer");
                goto cleanup;
            }
        }
    }

cleanup:
    if (dir != NULL) {
        closedir(dir);
    }

    free(path1);
    free(path2);

    return status;
}
