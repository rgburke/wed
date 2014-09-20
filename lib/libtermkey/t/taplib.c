#include "taplib.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int nexttest = 1;
static int _exit_status = 0;

void plan_tests(int n)
{
  printf("1..%d\n", n);
}

void pass(char *name)
{
  printf("ok %d - %s\n", nexttest++, name);
}

void fail(char *name)
{
  printf("not ok %d - %s\n", nexttest++, name);
  _exit_status = 1;
}

void ok(int cmp, char *name)
{
  if(cmp)
    pass(name);
  else
    fail(name);
}

void diag(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "# ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);
}

void is_int(int got, int expect, char *name)
{
  if(got == expect)
    ok(1, name);
  else {
    ok(0, name);
    diag("got %d expected %d", got, expect);
  }
}

void is_str(const char *got, const char *expect, char *name)
{
  if(strcmp(got, expect) == 0)
    ok(1, name);
  else {
    ok(0, name);
    diag("got '%s' expected '%s'", got, expect);
  }
}

int exit_status(void)
{
  return _exit_status;
}
