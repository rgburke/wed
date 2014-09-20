#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key1, key2;

  plan_tests(12);

  tk = termkey_new_abstract("vt100", 0);

  key1.type = TERMKEY_TYPE_UNICODE;
  key1.code.codepoint = 'A';
  key1.modifiers = 0;

  is_int(termkey_keycmp(tk, &key1, &key1), 0, "cmpkey same structure");

  key2.type = TERMKEY_TYPE_UNICODE;
  key2.code.codepoint = 'A';
  key2.modifiers = 0;

  is_int(termkey_keycmp(tk, &key1, &key2), 0, "cmpkey identical structure");

  key2.modifiers = TERMKEY_KEYMOD_CTRL;

  ok(termkey_keycmp(tk, &key1, &key2) < 0, "cmpkey orders CTRL after nomod");
  ok(termkey_keycmp(tk, &key2, &key1) > 0, "cmpkey orders nomod before CTRL");

  key2.code.codepoint = 'B';
  key2.modifiers = 0;

  ok(termkey_keycmp(tk, &key1, &key2) < 0, "cmpkey orders 'B' after 'A'");
  ok(termkey_keycmp(tk, &key2, &key1) > 0, "cmpkey orders 'A' before 'B'");

  key1.modifiers = TERMKEY_KEYMOD_CTRL;

  ok(termkey_keycmp(tk, &key1, &key2) < 0, "cmpkey orders nomod 'B' after CTRL 'A'");
  ok(termkey_keycmp(tk, &key2, &key1) > 0, "cmpkey orders CTRL 'A' before nomod 'B'");

  key2.type = TERMKEY_TYPE_KEYSYM;
  key2.code.sym = TERMKEY_SYM_UP;

  ok(termkey_keycmp(tk, &key1, &key2) < 0, "cmpkey orders KEYSYM after UNICODE");
  ok(termkey_keycmp(tk, &key2, &key1) > 0, "cmpkey orders UNICODE before KEYSYM");

  key1.type = TERMKEY_TYPE_KEYSYM;
  key1.code.sym = TERMKEY_SYM_SPACE;
  key1.modifiers = 0;
  key2.type = TERMKEY_TYPE_UNICODE;
  key2.code.codepoint = ' ';
  key2.modifiers = 0;

  is_int(termkey_keycmp(tk, &key1, &key2), 0, "cmpkey considers KEYSYM/SPACE and UNICODE/SP identical");

  termkey_set_canonflags(tk, termkey_get_canonflags(tk) | TERMKEY_CANON_SPACESYMBOL);
  is_int(termkey_keycmp(tk, &key1, &key2), 0, "cmpkey considers KEYSYM/SPACE and UNICODE/SP identical under SPACESYMBOL");

  termkey_destroy(tk);

  return exit_status();
}
