#ifndef WED_TAP_H
#define WED_TAP_H

/* A very basic and minimal TAP implementation */

#include <stddef.h>

#define plan tp_plan
#define ok(test,...) tp_ok(__FILE__,__LINE__,test,__VA_ARGS__)
#define msg tp_msg
#define exit_status tp_exit_status

void tp_plan(size_t);
int tp_ok(const char *, size_t, int, const char *, ...);
void tp_msg(const char *, ...);
int tp_exit_status(void);

#endif
