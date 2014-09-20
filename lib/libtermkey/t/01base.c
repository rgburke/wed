#include <stdio.h>
#include "../termkey.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TermKey   *tk;

  plan_tests(6);

  tk = termkey_new_abstract("vt100", 0);

  ok(!!tk, "termkey_new_abstract");

  is_int(termkey_get_buffer_size(tk), 256, "termkey_get_buffer_size");
  ok(termkey_is_started(tk), "termkey_is_started true after construction");

  termkey_stop(tk);

  ok(!termkey_is_started(tk), "termkey_is_started false after termkey_stop()");

  termkey_start(tk);

  ok(termkey_is_started(tk), "termkey_is_started true after termkey_start()");

  termkey_destroy(tk);

  ok(1, "termkey_free");

  return exit_status();
}
