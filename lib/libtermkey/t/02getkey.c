#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;

  plan_tests(31);

  tk = termkey_new_abstract("vt100", 0);

  is_int(termkey_get_buffer_remaining(tk), 256, "buffer free initially 256");

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_NONE, "getkey yields RES_NONE when empty");

  is_int(termkey_push_bytes(tk, "h", 1), 1, "push_bytes returns 1");

  is_int(termkey_get_buffer_remaining(tk), 255, "buffer free 255 after push_bytes");

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after h");

  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type after h");
  is_int(key.code.codepoint, 'h',                  "key.code.codepoint after h");
  is_int(key.modifiers,      0,                    "key.modifiers after h");
  is_str(key.utf8,           "h",                  "key.utf8 after h");

  is_int(termkey_get_buffer_remaining(tk), 256, "buffer free 256 after getkey");

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_NONE, "getkey yields RES_NONE a second time");

  termkey_push_bytes(tk, "\x01", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after C-a");

  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type after C-a");
  is_int(key.code.codepoint, 'a',                  "key.code.codepoint after C-a");
  is_int(key.modifiers,      TERMKEY_KEYMOD_CTRL,  "key.modifiers after C-a");

  termkey_push_bytes(tk, "\033OA", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after Up");

  is_int(key.type,        TERMKEY_TYPE_KEYSYM,  "key.type after Up");
  is_int(key.code.sym,    TERMKEY_SYM_UP,       "key.code.sym after Up");
  is_int(key.modifiers,   0,                    "key.modifiers after Up");

  is_int(termkey_push_bytes(tk, "\033O", 2), 2, "push_bytes returns 2");

  is_int(termkey_get_buffer_remaining(tk), 254, "buffer free 254 after partial write");

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN after partial write");

  termkey_push_bytes(tk, "C", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after Right completion");

  is_int(key.type,        TERMKEY_TYPE_KEYSYM,  "key.type after Right");
  is_int(key.code.sym,    TERMKEY_SYM_RIGHT,    "key.code.sym after Right");
  is_int(key.modifiers,   0,                    "key.modifiers after Right");

  is_int(termkey_get_buffer_remaining(tk), 256, "buffer free 256 after completion");

  termkey_push_bytes(tk, "\033[27;5u", 7);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY after Ctrl-Escape");

  is_int(key.type,        TERMKEY_TYPE_KEYSYM, "key.type after Ctrl-Escape");
  is_int(key.code.sym,    TERMKEY_SYM_ESCAPE,  "key.code.sym after Ctrl-Escape");
  is_int(key.modifiers,   TERMKEY_KEYMOD_CTRL, "key.modifiers after Ctrl-Escape");

  termkey_destroy(tk);

  return exit_status();
}
