#include <string.h>
#include "tap.h"

/* Use small gap to force gap expansion and test reallocing code */
#define GAP_INCREMENT 4

#include "../../gap_buffer.h"

static void gap_buffer_creation(GapBuffer *);
static void gap_buffer_insert(GapBuffer *, const char *, size_t);
static void gap_buffer_insert_2(GapBuffer *, const char *, size_t);
static void gap_buffer_movement(GapBuffer *);
static void gap_buffer_retrieval(GapBuffer *, const char *, size_t);
static void gap_buffer_delete(GapBuffer *);
static void gap_buffer_replace(GapBuffer *);
static void gap_buffer_clear(GapBuffer *);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    plan(74);

    GapBuffer *buffer = gb_new(GAP_INCREMENT);

    if (!ok(buffer != NULL, "Create GapBuffer")) {
        return exit_status();
    }

    const char *str = "This is test text.\nDon't change it.";
    size_t str_len = strlen(str);

    gap_buffer_creation(buffer);
    gap_buffer_insert(buffer, str, str_len);
    gap_buffer_insert_2(buffer, str, str_len);
    gap_buffer_movement(buffer);
    gap_buffer_retrieval(buffer, str, str_len);
    gap_buffer_delete(buffer);
    gap_buffer_replace(buffer);
    gap_buffer_clear(buffer);

    return exit_status();
}

static void gap_buffer_creation(GapBuffer *buffer)
{
    msg("Create:");
    ok(buffer->allocated == GAP_INCREMENT, "Memory allocated for buffer");
    ok(gb_gap_size(buffer) == buffer->allocated, "Gap size is all allocated space");
    ok(gb_get_point(buffer) == 0, "Point at buffer start");
    ok(gb_length(buffer) == 0, "Length is 0");
    ok(buffer->text != NULL, "Text field is set");
    ok(gb_lines(buffer) == 0, "No lines in buffer");
}

static void gap_buffer_insert(GapBuffer *buffer, const char *str, size_t str_len)
{
    msg("Insert:");
    ok(gb_add(buffer, str, str_len), "Insert text into buffer");
    ok(buffer->allocated >= str_len, "New space was allocated");
    ok(buffer->gap_start == str_len, "Gap start is at end of text");
    ok(buffer->gap_end == buffer->allocated, "Gap end is at end of allocated space");
    ok(gb_gap_size(buffer) == buffer->allocated - str_len, "Gap size is remaning free space");
    ok(gb_length(buffer) == str_len, "Buffer length is equal to string length");
    ok(gb_get_point(buffer) == str_len, "Point is at buffer end");
    ok(gb_lines(buffer) == 1, "1 line in buffer");
}

static void gap_buffer_insert_2(GapBuffer *buffer, const char *str, size_t str_len)
{
    msg("2nd Insert:");
    ok(gb_preallocate(buffer, str_len * 2), "Preallocate buffer space successful");
    ok(buffer->allocated >= str_len * 2, "New space was allocated");
    ok(gb_set_point(buffer, 0), "Set point to start of buffer");
    ok(gb_get_point(buffer) == 0, "Point is at buffer start");
    ok(gb_insert(buffer, str, str_len), "Insert text again into buffer");
    ok(buffer->gap_start == str_len, "Gap start is correct");
    ok(buffer->gap_end == buffer->allocated - str_len, "Gap end is correct");
    ok(gb_gap_size(buffer) == buffer->allocated - (str_len * 2), "Gap size is remaning free space");
    ok(gb_length(buffer) == str_len * 2, "Buffer length is equal to total string length");
    ok(gb_get_point(buffer) == 0, "Point is at buffer start");
    ok(gb_lines(buffer) == 2, "2 lines in buffer");
}

static void gap_buffer_movement(GapBuffer *buffer)
{
    msg("Movement:");
    ok(gb_set_point(buffer, 0), "Set point to buffer start");
    ok(buffer->point == 0, "Point is at buffer start");
    ok(gb_set_point(buffer, gb_length(buffer)), "Set point to buffer end");
    ok(gb_get_point(buffer) == gb_length(buffer), "Point is at buffer end");

    size_t point = 0;
    ok(gb_find_next(buffer, 0, &point, '\n'), "Found 1st new line");
    ok(gb_get_at(buffer, point) == '\n', "Point is at new line");
    ok(gb_find_next(buffer, point + 1, &point, '\n'), "Found 2nd new line");
    ok(gb_get_at(buffer, point) == '\n', "Point is at new line");
    ok(!gb_find_next(buffer, point + 1, &point, '\n'), "Cannot find further new line");
    ok(gb_find_prev(buffer, gb_length(buffer), &point, '\n'), "Found 2nd new line from end");
    ok(gb_get_at(buffer, point) == '\n', "Point is at new line");
    ok(gb_find_prev(buffer, point, &point, '\n'), "Found 1st new line from end");
    ok(gb_get_at(buffer, point) == '\n', "Point is at new line");
    ok(!gb_find_prev(buffer, point, &point, '\n'), "Cannot find further new line from end");
}

static void gap_buffer_retrieval(GapBuffer *buffer, const char *str, size_t str_len)
{
    msg("Retrieval:");
    int char_ret_success = 1;
    size_t buffer_len = gb_length(buffer);

    for (size_t k = 0; k < buffer_len; k++) {
        char_ret_success &= (gb_get_at(buffer, k) == str[k % str_len]);
    }

    ok(char_ret_success, "Char by char retrieval matches text");

    char buf[buffer_len];
    ok(gb_get_range(buffer, 0, buf, buffer_len) == buffer_len, "Retrieved text range from buffer");

    int matches_original = strncmp(buf, str, str_len) == 0 && 
                           strncmp(buf + str_len, str, str_len) == 0;

    ok(matches_original, "Text range retrieved matches original text");
}

static void gap_buffer_delete(GapBuffer *buffer)
{
    msg("Delete:");
    size_t point = 0;
    size_t buffer_len = gb_length(buffer);
    ok(gb_find_next(buffer, 0, &point, '\n'), "Found 1st new line");
    ok(gb_get_at(buffer, point) == '\n', "Point is at new line");
    ok(gb_set_point(buffer, point), "Point is at first new line");
    ok(gb_delete(buffer, buffer_len - point), "Deleting bytes");
    ok(buffer->gap_start == point, "Gap start is at point");
    ok(buffer->gap_end == buffer->allocated, "Gap end is at allocated space end");
    ok(gb_length(buffer) == point, "Buffer length decreased");
    ok(buffer->allocated - gb_gap_size(buffer) == gb_length(buffer), "All allocated space accounted for");
    ok(gb_lines(buffer) == 0, "No more lines in buffer");
}

static void gap_buffer_replace(GapBuffer *buffer)
{
    msg("Replace:");
    size_t buffer_len = gb_length(buffer);
    char buf_start[buffer_len], buf_end[buffer_len];
    memset(buf_start, 0, sizeof(buf_start));
    memset(buf_end, 0, sizeof(buf_end));
    ok(gb_get_range(buffer, 0, buf_start, buffer_len) == buffer_len, "Retrieved text range from buffer");

    ok(gb_set_point(buffer, 0), "Point is at buffer start");
    ok(gb_replace(buffer, 5, "", 0), "Replace first 5 bytes with empty string");
    ok(buffer_len - 5 == gb_length(buffer) && gb_get_at(buffer, 0) == 'i', "Replace with empty string correct");
    ok(gb_set_point(buffer, 0), "Point is at buffer start");
    ok(gb_replace(buffer, 0, "This ", 5), "Replace empty string with 5 bytes");
    ok(buffer_len == gb_length(buffer) && gb_get_at(buffer, 0) == 'T', "Replace empty string correct");
    ok(gb_set_point(buffer, 0), "Point is at buffer start");
    ok(gb_replace(buffer, 4, "is", 2), "Replace first 4 bytes with 2 bytes");
    ok(buffer_len - 2 == gb_length(buffer) && gb_get_at(buffer, 0) == 'i', "Replace with fewer bytes correct");
    ok(gb_set_point(buffer, 0), "Point is at buffer start");
    ok(gb_replace(buffer, 2, "This", 4), "Replaced first 2 bytes with 4 bytes");
    ok(buffer_len == gb_length(buffer) && gb_get_at(buffer, 0) == 'T', "Replace with more bytes correct");
    ok(gb_set_point(buffer, 0), "Point is at buffer start");
    ok(gb_replace(buffer, 4, "This", 4), "Replaced first 4 bytes with 4 bytes");
    ok(buffer_len == gb_length(buffer) && gb_get_at(buffer, 0) == 'T', "Replace with equal bytes correct");

    ok(gb_get_range(buffer, 0, buf_end, buffer_len) == buffer_len, "Retrieved text range from buffer");
    ok(strncmp(buf_start, buf_end, buffer_len) == 0, "Text range retrieved matches starting text");
}

static void gap_buffer_clear(GapBuffer *buffer)
{
    msg("Clear:");
    gb_clear(buffer);
    ok(gb_length(buffer) == 0, "Buffer is empty");
    ok(gb_get_point(buffer) == 0, "Point is at buffer start");
    ok(gb_lines(buffer) == 0, "No lines in buffer");
    ok(gb_gap_size(buffer) == buffer->allocated, "Gap size is all allocated space");
}
