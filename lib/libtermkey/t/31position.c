#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;
  int        line, col;

  plan_tests(8);

  tk = termkey_new_abstract("vt100", 0);

  termkey_push_bytes(tk, "\e[?15;7R", 8);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY for position report");

  is_int(key.type, TERMKEY_TYPE_POSITION, "key.type for position report");

  is_int(termkey_interpret_position(tk, &key, &line, &col), TERMKEY_RES_KEY, "interpret_position yields RES_KEY");

  is_int(line, 15, "line for position report");
  is_int(col,   7, "column for position report");

  /* A plain CSI R is likely to be <F3> though.
   * This is tricky :/
   */
  termkey_push_bytes(tk, "\e[R", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY for <F3>");

  is_int(key.type, TERMKEY_TYPE_FUNCTION, "key.type for <F3>");
  is_int(key.code.number, 3, "key.code.number for <F3>");

  termkey_destroy(tk);

  return exit_status();
}
