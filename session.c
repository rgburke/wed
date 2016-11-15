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
#include <fcntl.h>
#include <assert.h>
#include "session.h"
#include "status.h"
#include "util.h"
#include "buffer.h"
#include "command.h"
#include "config.h"
#include "build_config.h"
#include "tui.h"
#include "prompt_completer.h"

#define MAX_EMPTY_BUFFER_NAME_SIZE 20
#define FILE_TYPE_FILE_BUF_SIZE 128

static const char *se_get_empty_buffer_name(Session *);
static Status se_add_to_history(List *, const char *text);
static size_t se_populate_file_buf(const Buffer *, char *file_buf,
                                   size_t file_buf_size);
static void se_determine_filetype(Session *, Buffer *);
static void se_determine_fileformat(Session *, Buffer *);
static int se_is_valid_config_def(Session *, HashMap *, ConfigType,
                                  const char *def_name);
static int se_add_buffer_from_stdin(Session *);

Session *se_new(void)
{
    Session *sess = malloc(sizeof(Session));
    RETURN_IF_NULL(sess);
    memset(sess, 0, sizeof(Session));

    return sess;
}

int se_init(Session *sess, const WedOpt *wed_opt, char *buffer_paths[],
            int buffer_num)
{
    sess->wed_opt = *wed_opt;

    if ((sess->ui = ti_new(sess)) == NULL) {
        return 0;
    }

    if (!ip_init(&sess->input_buffer)) {
        return 0;
    }

    if (wed_opt->keystr_input != NULL) {
        if (!STATUS_IS_SUCCESS(
                ip_add_keystr_input_to_end(&sess->input_buffer,
                                           wed_opt->keystr_input,
                                           strlen(wed_opt->keystr_input)))) {
            return 0;
        }
    }

    if ((sess->error_buffer = bf_new_empty("errors", sess->config)) == NULL) {
        return 0;
    }

    if ((sess->msg_buffer = bf_new_empty("messages", sess->config)) == NULL) {
        return 0;
    }

    if ((sess->search_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->replace_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->command_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->lineno_history = list_new()) == NULL) {
        return 0;
    }

    if ((sess->buffer_history = list_new()) == NULL) {
        return 0;
    }

    if (!cm_init_key_map(&sess->key_map)) {
        return 0;
    }

    if ((sess->filetypes = new_hashmap()) == NULL) {
        return 0;
    }

    if (!sm_init(&sess->sm)) {
        return 0;
    }

    Buffer *prompt_buffer;

    if ((prompt_buffer = bf_new_empty("prompt", sess->config)) == NULL) {
        return 0;
    }

    if ((sess->prompt = pr_new(prompt_buffer)) == NULL) {
        return 0;
    }

    if ((sess->themes = new_hashmap()) == NULL) {
        return 0;
    }

    Theme *default_theme = th_get_default_theme();

    if (default_theme == NULL) {
        return 0;
    }

    if (!hashmap_set(sess->themes, "default", default_theme)) {
        return 0;
    }

    if ((sess->cfg_buffer_stack = list_new()) == NULL) {
        return 0;
    }

#if WED_FEATURE_LUA
    if ((sess->ls = ls_new(sess)) == 0) {
        return 0;
    }
#endif

    se_add_error(sess, cf_init_session_config(sess));

    if (sess->wed_opt.config_file_path != NULL) {
        se_add_error(sess, cf_load_config(sess,
                                          sess->wed_opt.config_file_path));
    }

#if WED_FEATURE_LUA
    se_add_error(sess, ls_init(sess->ls));
#endif

    if (buffer_num == 1 && strcmp("-", buffer_paths[0]) == 0) {
        if (!se_add_buffer_from_stdin(sess)) {
            warn("Failed to read from stdin");
            return 0;
        }
    } else {
        Status status;
        int buffer_index;

        for (int k = 0; k < buffer_num; k++) {
            status = se_get_buffer_index_by_path(sess, buffer_paths[k],
                                                 &buffer_index);

            if (STATUS_IS_SUCCESS(status) && buffer_index < 0) {
                status = se_add_new_buffer(sess, buffer_paths[k], 0);
            }

            se_add_error(sess, status);
        }
    }

    if (sess->buffer_num == 0) {
        se_add_new_empty_buffer(sess);
    }

    if (!se_set_active_buffer(sess, 0)) {
        return 0;
    }

    cl_init(&sess->clipboard);

    /* The prompt currently uses a single line, so don't wrap content */
    cf_set_var(CE_VAL(sess, prompt_buffer), CL_BUFFER,
               CV_LINEWRAP, INT_VAL(0));
    se_enable_msgs(sess);

    sess->initialised = 1;

    return 1;
}

void se_free(Session *sess)
{
    if (sess == NULL) {
        return;
    }

    Buffer *buffer = sess->buffers;
    Buffer *tmp;

    while (buffer != NULL) {
        tmp = buffer->next;
        bf_free(buffer);
        buffer = tmp;
    }

    ip_free(&sess->input_buffer);
    cm_free_key_map(&sess->key_map);
    cf_free_config(sess->config);
    pr_free(sess->prompt, 1);
    bf_free(sess->error_buffer);
    bf_free(sess->msg_buffer);
    list_free_all(sess->search_history);
    list_free_all(sess->replace_history);
    list_free_all(sess->command_history);
    list_free_all(sess->lineno_history);
    list_free_all(sess->buffer_history);
    free_hashmap_values(sess->filetypes, (void (*)(void *))ft_free);
    free_hashmap(sess->filetypes);
    free_hashmap_values(sess->themes, NULL);
    free_hashmap(sess->themes);
    list_free(sess->cfg_buffer_stack);
    cl_free(&sess->clipboard);
    sess->ui->free(sess->ui);
    sm_free(&sess->sm);

#if WED_FEATURE_LUA
    ls_free(sess->ls);
#endif

    free(sess);
}

int se_add_buffer(Session *sess, Buffer *buffer)
{
    assert(buffer != NULL);

    if (buffer == NULL) {
        return 0;
    }

    int re_enable_msgs = se_disable_msgs(sess);

    se_determine_filetype(sess, buffer);
    se_determine_syntaxtype(sess, buffer);
    se_determine_fileformat(sess, buffer);

    if (re_enable_msgs) {
        se_enable_msgs(sess);
    }

    sess->buffer_num++;

    if (sess->buffers == NULL) {
        sess->buffers = buffer;
        return 1;
    }

    Buffer *buff = sess->buffers;

    do {
        if (buff->next == NULL) {
            buff->next = buffer;
            break;
        }

        buff = buff->next;
    } while (1);
    
    return 1;
}

int se_is_valid_buffer_index(const Session *sess, size_t buffer_index)
{
    return sess->buffers != NULL &&
           buffer_index < sess->buffer_num; 
}

int se_get_buffer_index(const Session *sess, const Buffer *find_buffer,
                        size_t *buffer_index_ptr)
{
    const Buffer *buffer = sess->buffers;
    size_t buffer_index = 0;

    while (buffer != NULL) {
        if (buffer == find_buffer) {
            *buffer_index_ptr = buffer_index;
            return 1;
        }

        buffer = buffer->next;
        buffer_index++; 
    }

    return 0;
}

int se_set_active_buffer(Session *sess, size_t buffer_index)
{
    assert(se_is_valid_buffer_index(sess, buffer_index));

    if (sess->buffers == NULL || buffer_index >= sess->buffer_num) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    size_t iter = 0;

    while (iter < buffer_index) {
         buffer = buffer->next;
         iter++;
    }

    sess->active_buffer = buffer;
    sess->active_buffer_index = buffer_index;
    bf_set_is_draw_dirty(buffer, 1);

    return 1;
}

Buffer *se_get_buffer(const Session *sess, size_t buffer_index)
{
    assert(se_is_valid_buffer_index(sess, buffer_index));

    if (sess->buffers == NULL || buffer_index >= sess->buffer_num) {
        return NULL;
    }

    Buffer *buffer = sess->buffers;

    while (buffer_index-- != 0) {
        buffer = buffer->next;    
    }

    return buffer;
}

int se_remove_buffer(Session *sess, Buffer *to_remove)
{
    assert(sess->buffers != NULL);
    assert(to_remove != NULL);

    if (sess->buffers == NULL || to_remove == NULL) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    Buffer *prev = NULL;
    size_t buffer_index = 0;

    while (buffer != NULL && to_remove != buffer) {
        prev = buffer;    
        buffer = buffer->next;
        buffer_index++;
    }

    if (buffer == NULL) {
        return 0;
    }

    if (prev != NULL) {
        if (buffer->next != NULL) {
            prev->next = buffer->next; 

            if (sess->active_buffer_index == buffer_index) {
                sess->active_buffer = buffer->next;
            }
        } else {
            prev->next = NULL;
            sess->active_buffer = prev;

            if (sess->active_buffer_index == buffer_index) {
                sess->active_buffer_index--;
            }
        }
    } else if (sess->active_buffer == buffer) {
        if (buffer->next != NULL) {
            sess->buffers = buffer->next;
            sess->active_buffer = sess->buffers;
        } else {
            sess->buffers = NULL;
            sess->active_buffer = NULL;
        } 
    }

    sess->buffer_num--;

    bf_free(buffer);

    if (sess->active_buffer != NULL) {
        bf_set_is_draw_dirty(sess->active_buffer, 1);
    }

    return 1;
}

Status se_make_prompt_active(Session *sess, const PromptOpt *prompt_opt)
{
    RETURN_IF_FAIL(pr_reset_prompt(sess->prompt, prompt_opt));

    sess->key_map.active_op_modes[OM_PROMPT] = 1;

    if (pc_has_prompt_completer(prompt_opt->prompt_type)) {
        sess->key_map.active_op_modes[OM_PROMPT_COMPLETER] = 1;
    }

    Buffer *prompt_buffer = pr_get_prompt_buffer(sess->prompt);
    prompt_buffer->next = sess->active_buffer;
    sess->active_buffer = prompt_buffer;

    return STATUS_SUCCESS;
}

int se_end_prompt(Session *sess)
{
    assert(sess->active_buffer != NULL);

    if (sess->active_buffer == NULL) {
        return 0;
    }

    Buffer *prompt_buffer = pr_get_prompt_buffer(sess->prompt);

    assert(prompt_buffer != NULL);

    sess->active_buffer = prompt_buffer->next;

    sess->key_map.active_op_modes[OM_PROMPT] = 0;
    sess->key_map.active_op_modes[OM_PROMPT_COMPLETER] = 0;

    return 1;
}

int se_prompt_active(const Session *sess)
{
    assert(sess->active_buffer != NULL);

    if (sess->active_buffer == NULL) {
        return 0;
    }

    return sess->active_buffer == pr_get_prompt_buffer(sess->prompt);
}

void se_exclude_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types |= cmd_type;
}

void se_enable_command_type(Session *sess, CommandType cmd_type)
{
    sess->exclude_cmd_types &= ~cmd_type;
}

int se_command_type_excluded(const Session *sess, CommandType cmd_type)
{
    return sess->exclude_cmd_types & cmd_type;
}

int se_add_error(Session *sess, Status error)
{
    if (STATUS_IS_SUCCESS(error)) {
        return 0;
    }

    Buffer *error_buffer = sess->error_buffer;
    char error_msg[MAX_ERROR_MSG_SIZE];

    snprintf(error_msg, MAX_ERROR_MSG_SIZE, "Error %d: %s",
             error.error_code, error.msg);    
    st_free_status(error);

    /* Store each error message on its own line in the error buffer */
    if (!bp_at_buffer_start(&error_buffer->pos)) {
        bf_insert_character(error_buffer, "\n", 1);
    }

    bf_insert_string(error_buffer, error_msg,
                     strnlen(error_msg, MAX_ERROR_MSG_SIZE), 1);

    return 1;
}

int se_has_errors(const Session *sess)
{
    return !bf_is_empty(sess->error_buffer);
}

void se_clear_errors(Session *sess)
{
    bf_clear(sess->error_buffer);
}

int se_add_msg(Session *sess, const char *msg)
{
    assert(!is_null_or_empty(msg));

    if (msg == NULL) {
        return 0; 
    } else if (!se_msgs_enabled(sess)) {
        return 1;
    }

    Buffer *msg_buffer = sess->msg_buffer;

    if (!bp_at_buffer_start(&msg_buffer->pos)) {
        bf_insert_character(msg_buffer, "\n", 1);
    }

    bf_insert_string(msg_buffer, msg, strnlen(msg, MAX_MSG_SIZE), 1);

    return 1;
}

int se_has_msgs(const Session *sess)
{
    return !bf_is_empty(sess->msg_buffer);
}

void se_clear_msgs(Session *sess)
{
    bf_clear(sess->msg_buffer);
}

Status se_add_new_buffer(Session *sess, const char *file_path, int is_stdin)
{
    if (file_path == NULL || strnlen(file_path, 1) == 0) {
        return st_get_error(ERR_INVALID_FILE_PATH,
                            "Invalid file path - \"%s\"", file_path);
    }

    FileInfo file_info;
    Status status;
    Buffer *buffer = NULL;

    if (is_stdin) {
        RETURN_IF_FAIL(fi_init_stdin(&file_info, file_path));
    } else {
        RETURN_IF_FAIL(fi_init(&file_info, file_path));
    }

    if (fi_is_directory(&file_info)) {
        status = st_get_error(ERR_FILE_IS_DIRECTORY,
                              "%s is a directory", file_info.file_name);
        goto cleanup;
    } else if (!is_stdin && fi_is_special(&file_info)) {
        status = st_get_error(ERR_FILE_IS_SPECIAL,
                              "%s is not a regular file", file_info.file_name);
        goto cleanup;
    }

    buffer = bf_new(&file_info, sess->config);

    if (buffer == NULL) {
        status = OUT_OF_MEMORY("Unable to create buffer");
        goto cleanup;
    }

    status = bf_load_file(buffer);

    if (!STATUS_IS_SUCCESS(status)) {
        goto cleanup;
    }

    se_add_buffer(sess, buffer);

    return STATUS_SUCCESS;

cleanup:
    if (buffer == NULL) {
        fi_free(&file_info);
    } else {
        bf_free(buffer);
    }

    return status;
}

static const char *se_get_empty_buffer_name(Session *sess)
{
    static char empty_buf_name[MAX_EMPTY_BUFFER_NAME_SIZE];
    snprintf(empty_buf_name, MAX_EMPTY_BUFFER_NAME_SIZE,
             "[new %zu]", ++sess->empty_buffer_num);

    return empty_buf_name;
}

Status se_add_new_empty_buffer(Session *sess)
{
    const char *empty_buf_name = se_get_empty_buffer_name(sess);

    Buffer *buffer = bf_new_empty(empty_buf_name, sess->config);

    if (buffer == NULL) {
        return OUT_OF_MEMORY("Unable to create empty buffer");
    }   

    se_add_buffer(sess, buffer);

    return STATUS_SUCCESS;
}

Status se_get_buffer_index_by_path(const Session *sess, const char *file_path,
                                   int *buffer_index_ptr)
{
    assert(!is_null_or_empty(file_path));
    assert(buffer_index_ptr != NULL);

    FileInfo file_info;
    RETURN_IF_FAIL(fi_init(&file_info, file_path));

    Buffer *buffer = sess->buffers;
    *buffer_index_ptr = -1;
    int buffer_index = 0;

    while (buffer != NULL) {
        if (fi_equal(&buffer->file_info, &file_info)) {
            *buffer_index_ptr = buffer_index; 
            break;
        } 

        buffer = buffer->next;
        buffer_index++;
    }

    fi_free(&file_info);

    return STATUS_SUCCESS;
}

static Status se_add_to_history(List *history, const char *text)
{
    assert(text != NULL);

    /* Avoid empty text and duplicate entries in sequence */
    if (*text == '\0' ||
        (list_size(history) > 0 &&
         strcmp(list_get_last(history), text) == 0)) {
        return STATUS_SUCCESS;
    }

    char *text_copy = strdup(text);

    if (text_copy == NULL) {
        return OUT_OF_MEMORY("Unable save search history");
    }

    if (!list_add(history, text_copy)) {
        free(text_copy);
        return OUT_OF_MEMORY("Unable save search history");
    }

    return STATUS_SUCCESS;
}

Status se_add_search_to_history(Session *sess, const char *search_text)
{
    return se_add_to_history(sess->search_history, search_text);
}

Status se_add_replace_to_history(Session *sess, const char *replace_text)
{
    return se_add_to_history(sess->replace_history, replace_text);
}

Status se_add_cmd_to_history(Session *sess, const char *cmd_text)
{
    return se_add_to_history(sess->command_history, cmd_text);
}

Status se_add_lineno_to_history(Session *sess, const char *lineno_text)
{
    return se_add_to_history(sess->lineno_history, lineno_text);
}

Status se_add_buffer_to_history(Session *sess, const char *buffer_text)
{
    return se_add_to_history(sess->buffer_history, buffer_text);
}

Status se_add_filetype_def(Session *sess, FileType *file_type)
{
    assert(file_type != NULL);    

    FileType *existing = hashmap_get(sess->filetypes, file_type->name);

    if (!hashmap_set(sess->filetypes, file_type->name, file_type)) {
        return OUT_OF_MEMORY("Unable to save filetype");
    }

    if (existing != NULL) {
        ft_free(existing);
    }

    Buffer *buffer = sess->buffers;
    int re_enable_msgs = se_disable_msgs(sess);
    char file_buf[FILE_TYPE_FILE_BUF_SIZE];
    size_t file_buf_size;
    int matches;

    /* Check all existing buffer's without a filetype set 
     * to see if they match the newly added filetype */

    while (buffer != NULL) {
        if (is_null_or_empty(cf_string(buffer->config, CV_FILETYPE))) {
            file_buf_size = se_populate_file_buf(buffer, file_buf,
                                                 FILE_TYPE_FILE_BUF_SIZE);

            se_add_error(sess, ft_matches(file_type, &buffer->file_info,
                                          file_buf, file_buf_size, &matches));

            if (matches) {
                se_add_error(sess, cf_set_var(CE_VAL(sess, buffer), CL_BUFFER,
                                              CV_FILETYPE,
                                              STR_VAL(file_type->name)));
            }
        }

        buffer = buffer->next;
    }

    if (re_enable_msgs) {
        se_enable_msgs(sess);
    }

    return STATUS_SUCCESS;
}

static size_t se_populate_file_buf(const Buffer *buffer, char *file_buf,
                                   size_t file_buf_size)
{
    BufferPos pos_start = buffer->pos;
    bp_to_buffer_start(&pos_start);
    file_buf_size = bf_get_text(buffer, &pos_start, file_buf,
                                file_buf_size - 1);
    file_buf[file_buf_size] = '\0';

    if (file_buf_size == 0) {
        return 0;
    }

    /* If the regex below fails then file_buf contains an invalid UTF-8
     * sequence and there is no point attempting to run the
     * file_content regex against it */

    RegexResult regex_result;
    RegexInstance regex_instance;
    Regex regex = { .regex_pattern = ".", .modifiers = 0 };

    if (!STATUS_IS_SUCCESS(ru_compile(&regex_instance, &regex))) {
        return 0;
    }

    if (!STATUS_IS_SUCCESS(
                ru_exec(&regex_result, &regex_instance,
                        file_buf, file_buf_size, 0)
                )) {
        return 0;
    }

    return file_buf_size;
}

static void se_determine_filetype(Session *sess, Buffer *buffer)
{
    HashMap *filetypes = sess->filetypes;
    size_t key_num = hashmap_size(filetypes);
    const char **keys = hashmap_get_keys(filetypes);

    if (key_num == 0) {
        return;
    } else if (keys == NULL) {
        se_add_error(sess, OUT_OF_MEMORY("Unable to generate filetypes set"));
    }

    FileType *file_type;
    int matches;

    char file_buf[FILE_TYPE_FILE_BUF_SIZE];
    size_t file_buf_size = se_populate_file_buf(buffer, file_buf,
                                                FILE_TYPE_FILE_BUF_SIZE);

    for (size_t k = 0; k < key_num; k++) {
        file_type = hashmap_get(filetypes, keys[k]);

        if (file_type != NULL) {
            se_add_error(sess, ft_matches(file_type, &buffer->file_info,
                                          file_buf, file_buf_size, &matches));

            if (matches) {
                se_add_error(sess, cf_set_var(CE_VAL(sess, buffer), CL_BUFFER,
                                              CV_FILETYPE,
                                              STR_VAL(file_type->name)));
                break;
            }
        }
    }

    free(keys);
}

int se_msgs_enabled(const Session *sess)
{
    return sess->msgs_enabled;
}

int se_enable_msgs(Session *sess)
{
    int currently_enabled = sess->msgs_enabled;
    sess->msgs_enabled = 1;    
    return currently_enabled;
}

int se_disable_msgs(Session *sess)
{
    int currently_enabled = sess->msgs_enabled;
    sess->msgs_enabled = 0;
    return currently_enabled;
}

/* Attempt to set syntaxtype based on filetype if necessary */
void se_determine_syntaxtype(Session *sess, Buffer *buffer)
{
    if (!cf_bool(sess->config, CV_SYNTAX)) {
        return;
    }

    const char *file_type = cf_string(buffer->config, CV_FILETYPE);

    if (is_null_or_empty(file_type)) {
        return;
    }

    const char *syn_type = cf_string(buffer->config, CV_SYNTAXTYPE);

    if (!is_null_or_empty(syn_type) &&
        strcmp(syn_type, file_type) == 0) {
        return;
    }

    if (!se_is_valid_syntaxtype(sess, file_type)) {
        return;
    }

    se_add_error(sess, cf_set_var(CE_VAL(sess, buffer), CL_BUFFER, 
                                  CV_SYNTAXTYPE, STR_VAL((char *)file_type)));
}

static void se_determine_fileformat(Session *sess, Buffer *buffer)
{
    FileFormat file_format = bf_detect_fileformat(buffer);
    cf_set_var(CE_VAL(sess, buffer), CL_BUFFER, CV_FILEFORMAT, 
               STR_VAL((char *)bf_determine_fileformat_str(file_format)));
}

int se_is_valid_syntaxtype(Session *sess, const char *syn_type)
{
    if (is_null_or_empty(syn_type) || sm_has_def(&sess->sm, syn_type)) {
        return 1;
    }

    const char *sdt = cf_string(sess->config, CV_SYNTAXDEFTYPE);
    SyntaxDefinitionType type;

    if (!sm_get_syntax_definition_type(sdt, &type)) {
        return 0;
    }

    Status status = sm_load_definition(&sess->sm, sess, type, syn_type);

    if (!STATUS_IS_SUCCESS(status)) {
        st_free_status(status);
        return 0;
    }

    return sm_has_def(&sess->sm, syn_type);
}

static int se_is_valid_config_def(Session *sess, HashMap *defs, 
                                  ConfigType config_type, const char *def_name)
{
    const void *def = hashmap_get(defs, def_name);

    if (def != NULL) {
        return 1;
    }

    cf_load_config_def(sess, config_type, def_name);

    def = hashmap_get(defs, def_name);

    return def != NULL;
}

const SyntaxDefinition *se_get_syntax_def(const Session *sess,
                                          const Buffer *buffer)
{
    if (!cf_bool(sess->config, CV_SYNTAX)) {
        return NULL;
    }

    const char *syn_type = cf_string(buffer->config, CV_SYNTAXTYPE);

    return sm_get_def(&sess->sm, syn_type);
}

int se_is_valid_theme(Session *sess, const char *theme)
{
    return se_is_valid_config_def(sess, sess->themes, CT_THEME, theme);
}

Status se_add_theme(Session *sess, Theme *theme, const char *theme_name)
{
    assert(theme != NULL);    
    assert(!is_null_or_empty(theme_name));

    /* The default theme is always available in wed
     * and cannot be overwritten */
    if (strncmp(theme_name, "default", 8) == 0) {
        return st_get_error(ERR_OVERRIDE_DEFAULT_THEME, 
                            "Cannot override default theme");
    }

    Theme *existing = hashmap_get(sess->themes, theme_name);

    if (!hashmap_set(sess->themes, theme_name, theme)) {
        return OUT_OF_MEMORY("Unable to save theme definition");
    }

    if (existing != NULL) {
        free(existing);
    }

    return STATUS_SUCCESS;
}

const Theme *se_get_active_theme(const Session *sess)
{
    const char *theme_name = cf_string(sess->config, CV_THEME);

    assert(!is_null_or_empty(theme_name));
    
    const Theme *theme = hashmap_get(sess->themes, theme_name);

    assert(theme != NULL);

    return theme;
}

int se_initialised(const Session *sess)
{
    return sess->initialised;
}

void se_save_key(Session *sess, const char *key)
{
    snprintf(sess->prev_key, MAX_KEY_STR_SIZE, "%s", key);
}

const char *se_get_prev_key(const Session *sess)
{
    return sess->prev_key;
}

static int se_add_buffer_from_stdin(Session *sess)
{
    if (!STATUS_IS_SUCCESS(se_add_new_buffer(sess, "/dev/stdin", 1))) {
        return 0;
    }

    Buffer *buffer = sess->buffers;
    buffer->change_state.version++;
    FileInfo *file_info = &buffer->file_info;
    fi_free(file_info);
    
    if (!fi_init_empty(file_info, se_get_empty_buffer_name(sess))) {
        return 0; 
    }

    int fd = open("/dev/tty", O_RDONLY);

    if (fd == -1) {
        return 0;
    }

    int dup_success = (dup2(fd, STDIN_FILENO) != -1);

    close(fd);

    return dup_success;
}

int se_session_finished(const Session *sess)
{
    return sess->finished;
}

void se_set_session_finished(Session *sess)
{
    sess->finished = 1;
}

const char *se_get_file_type_display_name(const Session *sess,
                                          const Buffer *buffer)
{
    const char *file_type_name = cf_string(buffer->config, CV_FILETYPE);

    if (is_null_or_empty(file_type_name)) {
        return "";
    }

    const FileType *file_type = hashmap_get(sess->filetypes, file_type_name);

    if (file_type == NULL) {
        return "";
    }

    return file_type->display_name;
}

void se_determine_filetypes_if_unset(Session *sess, Buffer *buffer)
{
    int re_enable_msgs = se_disable_msgs(sess);

    if (is_null_or_empty(cf_string(buffer->config, CV_FILETYPE))) {
        se_determine_filetype(sess, buffer);
    }

    if (is_null_or_empty(cf_string(buffer->config, CV_SYNTAXTYPE))) {
        se_determine_syntaxtype(sess, buffer);
    }

    if (re_enable_msgs) {
        se_enable_msgs(sess);
    }
}

MouseClickEvent se_get_last_mouse_click_event(const Session *sess)
{
    return ip_get_last_mouse_click_event(&sess->input_buffer);
}

