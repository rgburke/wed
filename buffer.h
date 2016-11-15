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

#ifndef WED_BUFFER_H
#define WED_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include "shared.h"
#include "status.h"
#include "file.h"
#include "hashmap.h"
#include "encoding.h"
#include "gap_buffer.h"
#include "buffer_pos.h"
#include "search.h"
#include "undo.h"
#include "regex_util.h"
#include "external_command.h"
#include "syntax.h"
#include "buffer_view.h"

/* Character classification */
typedef enum {
    CCLASS_WHITESPACE,
    CCLASS_PUNCTUATION,
    CCLASS_WORD
} CharacterClass;

/* Movement in buffer */
typedef enum {
    DIRECTION_NONE,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_WITH_SELECT = 1 << 3
} Direction;

/* Store cut/copied text when using wed-clipboard is not possible */
typedef struct {
    FileFormat file_format;
    char *str;
    size_t str_len;
} TextSelection;

typedef struct Buffer Buffer;

/* The in memory representation of a file */
struct Buffer {
    FileInfo file_info; /* stat like info */
    BufferPos pos; /* The cursor position */
    BufferPos select_start; /* Starting position of selected text */
    Buffer *next; /* Next buffer in this session */
    size_t line_col_offset; /* Global cursor line offset */
    HashMap *config; /* Stores config variables */
    BufferChangeState change_state; /* Reference to state when buffer was
                                       last written */
    int is_draw_dirty; /* Any modification performed since last draw */
    GapBuffer *data; /* Gap Buffer which stores buffer content */
    BufferSearch search; /* Search params */
    BufferChanges changes; /* Undo/Redo */
    FileFormat file_format; /* Unix or Windows line endings */
    RegexInstance mask; /* Inserted text can match mask */
    HashMap *marks; /* Buffer marks */
    BufferView *bv; /* In memory display of buffer */
};

/* The following two stream implementations make it possible to filter buffer
 * content through an external command */

/* Implements an InputStream allowing reading from buffer as a stream */
typedef struct {
    InputStream is;
    Buffer *buffer;
    BufferPos end_pos;
    BufferPos read_pos;
} BufferInputStream;

/* Implements an OutputStream allowing the buffer to written to as a stream */
typedef struct {
    OutputStream os;
    Buffer *buffer;
    BufferPos write_pos;
    int replace_mode;
} BufferOutputStream;

Buffer *bf_new(const FileInfo *, const HashMap *config);
Buffer *bf_new_empty(const char *, const HashMap *config);
void bf_free(Buffer *);
void bf_free_syntax_match_cache(Buffer *);
Status bf_clear(Buffer *);
Status bf_reset(Buffer *);
FileFormat bf_detect_fileformat(const Buffer *);
Status bf_load_file(Buffer *);
Status bf_read_file(Buffer *, const FileInfo *);
Status bf_write_file(Buffer *, const char *file_path);
char *bf_to_string(const Buffer *);
char *bf_join_lines_string(const Buffer *, const char *seperator);
int bf_is_empty(const Buffer *);
size_t bf_lines(const Buffer *);
size_t bf_length(const Buffer *);
int bf_is_view_initialised(const Buffer *);
int bf_is_dirty(const Buffer *);
int bf_is_draw_dirty(const Buffer *);
void bf_set_is_draw_dirty(Buffer *, int);
int bf_get_range(Buffer *, Range *);
int bf_bp_in_range(const Range *, const BufferPos *);
int bf_offset_in_range(const Range *, size_t offset);
Status bf_get_buffer_input_stream(BufferInputStream *, Buffer *, const Range *);
Status bf_get_buffer_output_stream(BufferOutputStream *, Buffer *,
                                   const BufferPos *write_pos,
                                   int replace_mode);
CharacterClass bf_character_class(const Buffer *, const BufferPos *);
FileFormat bf_get_fileformat(const Buffer *);
int bf_determine_fileformat(const char *ff_name, FileFormat *);
const char *bf_determine_fileformat_str(FileFormat);
void bf_set_fileformat(Buffer *, FileFormat);
const char *bf_new_line_str(FileFormat);
int bf_bp_at_screen_line_start(const Buffer *, const BufferPos *);
int bf_bp_at_screen_line_end(const Buffer *, const BufferPos *);
int bf_bp_move_past_buffer_extremes(const BufferPos *, Direction);
int bf_selection_started(const Buffer *);
Status bf_set_bp(Buffer *, const BufferPos *, int is_select);
Status bf_change_line(Buffer *, BufferPos *, Direction, int is_cursor);
Status bf_change_multi_line(Buffer *, BufferPos *, Direction, 
                            size_t offset, int is_cursor);
Status bf_change_char(Buffer *, BufferPos *, Direction, int is_cursor);
Status bf_change_multi_char(Buffer *, BufferPos *, Direction, 
                            size_t offset, int is_cursor);
Status bf_bp_to_line_start(Buffer *, BufferPos *, int is_select, int is_cursor);
Status bf_bp_to_screen_line_start(Buffer *, BufferPos *, 
                                  int is_select, int is_cursor);
Status bf_to_line_start(Buffer *, BufferPos *, int is_select, int is_cursor);
Status bf_to_line_end(Buffer *, int is_select);
Status bf_bp_to_line_end(Buffer *, BufferPos *, int is_select, int is_cursor);
Status bf_bp_to_screen_line_end(Buffer *, BufferPos *, 
                                int is_select, int is_cursor);
Status bf_to_next_word(Buffer *, int is_select);
Status bf_to_prev_word(Buffer *, int is_select);
Status bf_to_next_paragraph(Buffer *, int is_select);
Status bf_to_prev_paragraph(Buffer *, int is_select);
Status bf_change_page(Buffer *, Direction);
Status bf_to_buffer_start(Buffer *, int is_select);
Status bf_to_buffer_end(Buffer *, int is_select);
Status bf_add_new_mark(Buffer *, BufferPos *, MarkProperties);
Status bf_insert_character(Buffer *, const char *character, int advance_cursor);
Status bf_insert_string(Buffer *, const char *string, 
                        size_t string_length, int advance_cursor);
Status bf_replace_string(Buffer *, size_t replace_length, const char *string, 
                         size_t string_length, int advance_cursor);
Status bf_delete(Buffer *, size_t byte_num);
Status bf_delete_character(Buffer *);
Status bf_select_continue(Buffer *);
Status bf_select_reset(Buffer *);
Status bf_delete_range(Buffer *, const Range *);
Status bf_select_all_text(Buffer *);
Status bf_copy_selected_text(Buffer *, TextSelection *);
Status bf_cut_selected_text(Buffer *, TextSelection *);
Status bf_insert_textselection(Buffer *, TextSelection *, int advance_cursor);
void bf_free_textselection(TextSelection *);
Status bf_delete_word(Buffer *);
Status bf_delete_prev_word(Buffer *);
Status bf_set_text(Buffer *, const char *text);
Status bf_reset_with_text(Buffer *, const char *text);
Status bf_set_mask(Buffer *, const Regex *);
int bf_has_mask(const Buffer *);
void bf_remove_mask(Buffer *);
Status bf_goto_line(Buffer *, size_t line_no);
Status bf_vert_move_lines(Buffer *, Direction);
Status bf_indent(Buffer *, Direction);
Status bf_jump_to_matching_bracket(Buffer *);
Status bf_duplicate_selection(Buffer *);
Status bf_join_lines(Buffer *, const char *sep, size_t sep_len);
size_t bf_get_text(const Buffer *, const BufferPos *, char *buf,
                   size_t text_len);

#endif
