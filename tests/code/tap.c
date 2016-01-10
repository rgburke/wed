#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "tap.h"

static size_t tp_test_num = 0;
static size_t tp_current_test = 0;
static int tp_exit_code = 0;

void tp_plan(size_t test_num)
{
    if (test_num == 0) {
        printf("1..0\n");
        exit(0);
    }

    tp_test_num = test_num;
    printf("1..%zu\n", tp_test_num);
}

int tp_ok(const char *file, size_t line_no, int test, const char *fmt, ...)
{
    tp_current_test++;

    if (test) {
        printf("ok %zu", tp_current_test);
    } else {
        tp_exit_code = 1;
        printf("not ok %zu", tp_current_test);
    }

    if (fmt == NULL || *fmt == '\0') {
        return test; 
    }

    printf(" - ");

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vprintf(fmt, arg_ptr);
    va_end(arg_ptr); 

    printf("\n");

    if (!test) {
        tp_msg("Test %zu failed at %s:%zu", tp_current_test, file, line_no);
    }

    return test;
}

void tp_msg(const char *fmt, ...)
{
    printf("# ");
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vprintf(fmt, arg_ptr);
    va_end(arg_ptr); 
    printf("\n");
}

int tp_exit_status(void)
{
    if (tp_current_test != tp_test_num) {
        tp_msg("WARNING: Ran %zu tests but planned %zu", tp_current_test, tp_test_num);
    }

    return tp_exit_code;
}
