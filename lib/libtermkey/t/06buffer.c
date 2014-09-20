#include <stdio.h>
#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;
  TermKeyKey key;

  plan_tests(9);

  tk = termkey_new_abstract("vt100", 0);

  is_int(termkey_get_buffer_remaining(tk), 256, "buffer free initially 256");
  is_int(termkey_get_buffer_size(tk),      256, "buffer size initially 256");

  is_int(termkey_push_bytes(tk, "h", 1), 1, "push_bytes returns 1");

  is_int(termkey_get_buffer_remaining(tk), 255, "buffer free 255 after push_bytes");
  is_int(termkey_get_buffer_size(tk),      256, "buffer size 256 after push_bytes");

  ok(!!termkey_set_buffer_size(tk, 512), "buffer set size OK");

  is_int(termkey_get_buffer_remaining(tk), 511, "buffer free 511 after push_bytes");
  is_int(termkey_get_buffer_size(tk),      512, "buffer size 512 after push_bytes");

  is_int(termkey_getkey(tk, &key), TERMKEY_RES_KEY, "buffered key still useable after resize");

  termkey_destroy(tk);

  return exit_status();
}
