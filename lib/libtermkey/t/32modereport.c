#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;
  int        initial, mode, value;

  plan_tests(12);

  tk = termkey_new_abstract("vt100", 0);

  termkey_push_bytes(tk, "\e[?1;2$y", 8);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY for mode report");

  is_int(key.type, TERMKEY_TYPE_MODEREPORT, "key.type for mode report");

  is_int(termkey_interpret_modereport(tk, &key, &initial, &mode, &value), TERMKEY_RES_KEY, "interpret_modereoprt yields RES_KEY");

  is_int(initial, '?', "initial indicator from mode report");
  is_int(mode,      1, "mode number from mode report");
  is_int(value,     2, "mode value from mode report");

  termkey_push_bytes(tk, "\e[4;1$y", 7);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY for mode report");

  is_int(key.type, TERMKEY_TYPE_MODEREPORT, "key.type for mode report");

  is_int(termkey_interpret_modereport(tk, &key, &initial, &mode, &value), TERMKEY_RES_KEY, "interpret_modereoprt yields RES_KEY");

  is_int(initial, 0, "initial indicator from mode report");
  is_int(mode,    4, "mode number from mode report");
  is_int(value,   1, "mode value from mode report");

  termkey_destroy(tk);

  return exit_status();
}
