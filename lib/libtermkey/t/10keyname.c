#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeySym sym;
  const char *end;

  plan_tests(10);

  tk = termkey_new_abstract("vt100", 0);

  sym = termkey_keyname2sym(tk, "Space");
  is_int(sym, TERMKEY_SYM_SPACE, "keyname2sym Space");

  sym = termkey_keyname2sym(tk, "SomeUnknownKey");
  is_int(sym, TERMKEY_SYM_UNKNOWN, "keyname2sym SomeUnknownKey");

  end = termkey_lookup_keyname(tk, "Up", &sym);
  ok(!!end, "termkey_get_keyname Up returns non-NULL");
  is_str(end, "", "termkey_get_keyname Up return points at endofstring");
  is_int(sym, TERMKEY_SYM_UP, "termkey_get_keyname Up yields Up symbol");

  end = termkey_lookup_keyname(tk, "DownMore", &sym);
  ok(!!end, "termkey_get_keyname DownMore returns non-NULL");
  is_str(end, "More", "termkey_get_keyname DownMore return points at More");
  is_int(sym, TERMKEY_SYM_DOWN, "termkey_get_keyname DownMore yields Down symbol");

  end = termkey_lookup_keyname(tk, "SomeUnknownKey", &sym);
  ok(!end, "termkey_get_keyname SomeUnknownKey returns NULL");

  is_str(termkey_get_keyname(tk, TERMKEY_SYM_SPACE), "Space", "get_keyname SPACE");

  termkey_destroy(tk);

  return exit_status();
}
