#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey      *tk;
  TermKeyKey    key;
  const char   *endp;

#define CLEAR_KEY do { key.type = -1; key.code.codepoint = -1; key.modifiers = -1; key.utf8[0] = 0; } while(0)

  plan_tests(26);

  tk = termkey_new_abstract("vt100", 0);

  CLEAR_KEY;
  endp = termkey_strpkey(tk, " ", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for SP/unicode");
  is_int(key.code.codepoint, ' ',               "key.code.codepoint for SP/unicode");
  is_int(key.modifiers,   0,                    "key.modifiers for SP/unicode");
  is_str(key.utf8,        " ",                  "key.utf8 for SP/unicode");
  is_str(endp, "", "consumed entire input for SP/unicode");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Space", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for Space/unicode");
  is_int(key.code.codepoint, ' ',               "key.code.codepoint for Space/unicode");
  is_int(key.modifiers,   0,                    "key.modifiers for Space/unicode");
  is_str(key.utf8,        " ",                  "key.utf8 for Space/unicode");
  is_str(endp, "", "consumed entire input for Space/unicode");

  termkey_set_canonflags(tk, termkey_get_canonflags(tk) | TERMKEY_CANON_SPACESYMBOL);

  CLEAR_KEY;
  endp = termkey_strpkey(tk, " ", &key, 0);
  is_int(key.type,      TERMKEY_TYPE_KEYSYM, "key.type for SP/symbol");
  is_int(key.code.sym,  TERMKEY_SYM_SPACE,   "key.code.codepoint for SP/symbol");
  is_int(key.modifiers, 0,                   "key.modifiers for SP/symbol");
  is_str(endp, "", "consumed entire input for SP/symbol");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Space", &key, 0);
  is_int(key.type,      TERMKEY_TYPE_KEYSYM, "key.type for Space/symbol");
  is_int(key.code.sym,  TERMKEY_SYM_SPACE,   "key.code.codepoint for Space/symbol");
  is_int(key.modifiers, 0,                   "key.modifiers for Space/symbol");
  is_str(endp, "", "consumed entire input for Space/symbol");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "DEL", &key, 0);
  is_int(key.type,      TERMKEY_TYPE_KEYSYM, "key.type for Del/unconverted");
  is_int(key.code.sym,  TERMKEY_SYM_DEL,     "key.code.codepoint for Del/unconverted");
  is_int(key.modifiers, 0,                   "key.modifiers for Del/unconverted");
  is_str(endp, "", "consumed entire input for Del/unconverted");

  termkey_set_canonflags(tk, termkey_get_canonflags(tk) | TERMKEY_CANON_DELBS);

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "DEL", &key, 0);
  is_int(key.type,      TERMKEY_TYPE_KEYSYM,   "key.type for Del/as-backspace");
  is_int(key.code.sym,  TERMKEY_SYM_BACKSPACE, "key.code.codepoint for Del/as-backspace");
  is_int(key.modifiers, 0,                     "key.modifiers for Del/as-backspace");
  is_str(endp, "", "consumed entire input for Del/as-backspace");

  termkey_destroy(tk);

  return exit_status();
}
