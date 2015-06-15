#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;

  plan_tests(57);

  tk = termkey_new_abstract("vt100", TERMKEY_FLAG_UTF8);

  termkey_push_bytes(tk, "a", 1);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY low ASCII");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type low ASCII");
  is_int(key.code.codepoint, 'a',                  "key.code.codepoint low ASCII");

  /* 2-byte UTF-8 range is U+0080 to U+07FF (0xDF 0xBF) */
  /* However, we'd best avoid the C1 range, so we'll start at U+00A0 (0xC2 0xA0) */

  termkey_push_bytes(tk, "\xC2\xA0", 2);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 2 low");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 2 low");
  is_int(key.code.codepoint, 0x00A0,               "key.code.codepoint UTF-8 2 low");

  termkey_push_bytes(tk, "\xDF\xBF", 2);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 2 high");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 2 high");
  is_int(key.code.codepoint, 0x07FF,               "key.code.codepoint UTF-8 2 high");

  /* 3-byte UTF-8 range is U+0800 (0xE0 0xA0 0x80) to U+FFFD (0xEF 0xBF 0xBD) */

  termkey_push_bytes(tk, "\xE0\xA0\x80", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 low");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 3 low");
  is_int(key.code.codepoint, 0x0800,               "key.code.codepoint UTF-8 3 low");

  termkey_push_bytes(tk, "\xEF\xBF\xBD", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 high");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 3 high");
  is_int(key.code.codepoint, 0xFFFD,               "key.code.codepoint UTF-8 3 high");

  /* 4-byte UTF-8 range is U+10000 (0xF0 0x90 0x80 0x80) to U+10FFFF (0xF4 0x8F 0xBF 0xBF) */

  termkey_push_bytes(tk, "\xF0\x90\x80\x80", 4);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 low");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 4 low");
  is_int(key.code.codepoint, 0x10000,              "key.code.codepoint UTF-8 4 low");

  termkey_push_bytes(tk, "\xF4\x8F\xBF\xBF", 4);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 high");
  is_int(key.type,           TERMKEY_TYPE_UNICODE, "key.type UTF-8 4 high");
  is_int(key.code.codepoint, 0x10FFFF,             "key.code.codepoint UTF-8 4 high");

  /* Invalid continuations */

  termkey_push_bytes(tk, "\xC2!", 2);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 2 invalid cont");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 2 invalid cont");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 2 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 2 invalid after");

  termkey_push_bytes(tk, "\xE0!", 2);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 invalid cont");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 3 invalid cont");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 3 invalid after");

  termkey_push_bytes(tk, "\xE0\xA0!", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 invalid cont 2");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 3 invalid cont 2");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 3 invalid after");

  termkey_push_bytes(tk, "\xF0!", 2);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid cont");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 4 invalid cont");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 4 invalid after");

  termkey_push_bytes(tk, "\xF0\x90!", 3);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid cont 2");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 4 invalid cont 2");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 4 invalid after");

  termkey_push_bytes(tk, "\xF0\x90\x80!", 4);

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid cont 3");
  is_int(key.code.codepoint, 0xFFFD, "key.code.codepoint UTF-8 4 invalid cont 3");
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 invalid after");
  is_int(key.code.codepoint, '!', "key.code.codepoint UTF-8 4 invalid after");

  /* Partials */

  termkey_push_bytes(tk, "\xC2", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 2 partial");

  termkey_push_bytes(tk, "\xA0", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 2 partial");
  is_int(key.code.codepoint, 0x00A0, "key.code.codepoint UTF-8 2 partial");

  termkey_push_bytes(tk, "\xE0", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 3 partial");

  termkey_push_bytes(tk, "\xA0", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 3 partial");

  termkey_push_bytes(tk, "\x80", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 3 partial");
  is_int(key.code.codepoint, 0x0800, "key.code.codepoint UTF-8 3 partial");

  termkey_push_bytes(tk, "\xF0", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 4 partial");

  termkey_push_bytes(tk, "\x90", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 4 partial");

  termkey_push_bytes(tk, "\x80", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_AGAIN, "getkey yields RES_AGAIN UTF-8 4 partial");

  termkey_push_bytes(tk, "\x80", 1);
  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "getkey yields RES_KEY UTF-8 4 partial");
  is_int(key.code.codepoint, 0x10000, "key.code.codepoint UTF-8 4 partial");

  termkey_destroy(tk);

  return exit_status();
}
