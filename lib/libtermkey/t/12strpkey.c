#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey      *tk;
  TermKeyKey    key;
  const char   *endp;

#define CLEAR_KEY do { key.type = -1; key.code.codepoint = -1; key.modifiers = -1; key.utf8[0] = 0; } while(0)

  plan_tests(62);

  tk = termkey_new_abstract("vt100", 0);

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "A", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/A/0");
  is_int(key.code.codepoint, 'A',               "key.code.codepoint for unicode/A/0");
  is_int(key.modifiers,   0,                    "key.modifiers for unicode/A/0");
  is_str(key.utf8,        "A",                  "key.utf8 for unicode/A/0");
  is_str(endp, "", "consumed entire input for unicode/A/0");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "A and more", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/A/0 trailing");
  is_int(key.code.codepoint, 'A',               "key.code.codepoint for unicode/A/0 trailing");
  is_int(key.modifiers,   0,                    "key.modifiers for unicode/A/0 trailing");
  is_str(key.utf8,        "A",                  "key.utf8 for unicode/A/0 trailing");
  is_str(endp, " and more", "points at string tail for unicode/A/0 trailing");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "C-b", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/b/CTRL");
  is_int(key.code.codepoint, 'b',               "key.code.codepoint for unicode/b/CTRL");
  is_int(key.modifiers,   TERMKEY_KEYMOD_CTRL,  "key.modifiers for unicode/b/CTRL");
  is_str(key.utf8,        "b",                  "key.utf8 for unicode/b/CTRL");
  is_str(endp, "", "consumed entire input for unicode/b/CTRL");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Ctrl-b", &key, TERMKEY_FORMAT_LONGMOD);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/b/CTRL longmod");
  is_int(key.code.codepoint, 'b',               "key.code.codepoint for unicode/b/CTRL longmod");
  is_int(key.modifiers,   TERMKEY_KEYMOD_CTRL,  "key.modifiers for unicode/b/CTRL longmod");
  is_str(key.utf8,        "b",                  "key.utf8 for unicode/b/CTRL longmod");
  is_str(endp, "", "consumed entire input for unicode/b/CTRL longmod");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "^B", &key, TERMKEY_FORMAT_CARETCTRL);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/b/CTRL caretctrl");
  is_int(key.code.codepoint, 'b',               "key.code.codepoint for unicode/b/CTRL caretctrl");
  is_int(key.modifiers,   TERMKEY_KEYMOD_CTRL,  "key.modifiers for unicode/b/CTRL caretctrl");
  is_str(key.utf8,        "b",                  "key.utf8 for unicode/b/CTRL caretctrl");
  is_str(endp, "", "consumed entire input for unicode/b/CTRL caretctrl");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "A-c", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/c/ALT");
  is_int(key.code.codepoint, 'c',               "key.code.codepoint for unicode/c/ALT");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT,   "key.modifiers for unicode/c/ALT");
  is_str(key.utf8,        "c",                  "key.utf8 for unicode/c/ALT");
  is_str(endp, "", "consumed entire input for unicode/c/ALT");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Alt-c", &key, TERMKEY_FORMAT_LONGMOD);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/c/ALT longmod");
  is_int(key.code.codepoint, 'c',               "key.code.codepoint for unicode/c/ALT longmod");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT,   "key.modifiers for unicode/c/ALT longmod");
  is_str(key.utf8,        "c",                  "key.utf8 for unicode/c/ALT longmod");
  is_str(endp, "", "consumed entire input for unicode/c/ALT longmod");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "M-c", &key, TERMKEY_FORMAT_ALTISMETA);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/c/ALT altismeta");
  is_int(key.code.codepoint, 'c',               "key.code.codepoint for unicode/c/ALT altismeta");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT,   "key.modifiers for unicode/c/ALT altismeta");
  is_str(key.utf8,        "c",                  "key.utf8 for unicode/c/ALT altismeta");
  is_str(endp, "", "consumed entire input for unicode/c/ALT altismeta");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Meta-c", &key, TERMKEY_FORMAT_ALTISMETA|TERMKEY_FORMAT_LONGMOD);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/c/ALT altismeta+longmod");
  is_int(key.code.codepoint, 'c',               "key.code.codepoint for unicode/c/ALT altismeta+longmod");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT,   "key.modifiers for unicode/c/ALT altismeta+longmod");
  is_str(key.utf8,        "c",                  "key.utf8 for unicode/c/ALT altismeta+longmod");
  is_str(endp, "", "consumed entire input for unicode/c/ALT altismeta+longmod");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "meta c", &key, TERMKEY_FORMAT_ALTISMETA|TERMKEY_FORMAT_LONGMOD|TERMKEY_FORMAT_SPACEMOD|TERMKEY_FORMAT_LOWERMOD);
  is_int(key.type,        TERMKEY_TYPE_UNICODE, "key.type for unicode/c/ALT altismeta+long/space+lowermod");
  is_int(key.code.codepoint, 'c',               "key.code.codepoint for unicode/c/ALT altismeta+long/space+lowermod");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT,   "key.modifiers for unicode/c/ALT altismeta+long/space+lowermod");
  is_str(key.utf8,        "c",                  "key.utf8 for unicode/c/ALT altismeta+long/space_lowermod");
  is_str(endp, "", "consumed entire input for unicode/c/ALT altismeta+long/space+lowermod");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "ctrl alt page up", &key, TERMKEY_FORMAT_LONGMOD|TERMKEY_FORMAT_SPACEMOD|TERMKEY_FORMAT_LOWERMOD|TERMKEY_FORMAT_LOWERSPACE);
  is_int(key.type,        TERMKEY_TYPE_KEYSYM, "key.type for sym/PageUp/CTRL+ALT long/space/lowermod+lowerspace");
  is_int(key.code.sym,    TERMKEY_SYM_PAGEUP,  "key.code.codepoint for sym/PageUp/CTRL+ALT long/space/lowermod+lowerspace");
  is_int(key.modifiers,   TERMKEY_KEYMOD_ALT | TERMKEY_KEYMOD_CTRL,
                                               "key.modifiers for sym/PageUp/CTRL+ALT long/space/lowermod+lowerspace");
  is_str(endp, "", "consumed entire input for sym/PageUp/CTRL+ALT long/space/lowermod+lowerspace");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "Up", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_KEYSYM, "key.type for sym/Up/0");
  is_int(key.code.sym,    TERMKEY_SYM_UP,      "key.code.codepoint for sym/Up/0");
  is_int(key.modifiers,   0,                   "key.modifiers for sym/Up/0");
  is_str(endp, "", "consumed entire input for sym/Up/0");

  CLEAR_KEY;
  endp = termkey_strpkey(tk, "F5", &key, 0);
  is_int(key.type,        TERMKEY_TYPE_FUNCTION, "key.type for func/5/0");
  is_int(key.code.number, 5,                     "key.code.number for func/5/0");
  is_int(key.modifiers,   0,                     "key.modifiers for func/5/0");
  is_str(endp, "", "consumed entire input for func/5/0");

  termkey_destroy(tk);

  return exit_status();
}
