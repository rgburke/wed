#include <stdio.h>
#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;

  plan_tests(8);

  tk = termkey_new_abstract("vt100", 0);

  termkey_push_bytes(tk, " ", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after space");

  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type after space");
  is_int(key.code.codepoint, ' ',                  "key.code.codepoint after space");
  is_int(key.modifiers,      0,                    "key.modifiers after space");

  termkey_set_flags(tk, TERMKEY_FLAG_SPACESYMBOL);

  termkey_push_bytes(tk, " ", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after space");

  is_int(key.type,      TERMKEY_TYPE_KEYSYM, "key.type after space with FLAG_SPACESYMBOL");
  is_int(key.code.sym,  TERMKEY_SYM_SPACE,   "key.code.sym after space with FLAG_SPACESYMBOL");
  is_int(key.modifiers, 0,                   "key.modifiers after space with FLAG_SPACESYMBOL");

  termkey_destroy(tk);

  return exit_status();
}
