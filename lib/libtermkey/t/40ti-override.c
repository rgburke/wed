#include "../termkey.h"
#include "taplib.h"

#include <stdio.h>

#include <string.h>

#define streq(a,b) (!strcmp(a,b))

static const char *backspace_is_X(const char *name, const char *val, void *_)
{
  if(streq(name, "key_backspace"))
    return "X";

  return val;
}

int main(int argc, char *argv[])
{
  TermKey    *tk;
  TermKeyKey  key;

  plan_tests(3);

  tk = termkey_new_abstract("vt100", TERMKEY_FLAG_NOSTART);
  termkey_hook_terminfo_getstr(tk, &backspace_is_X, NULL);
  termkey_start(tk);

  termkey_push_bytes(tk, "X", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after X");

  is_int(key.type,     TERMKEY_TYPE_KEYSYM,   "key.type after X");
  is_int(key.code.sym, TERMKEY_SYM_BACKSPACE, "key.code.sym after X");

  termkey_destroy(tk);

  return exit_status();
}
