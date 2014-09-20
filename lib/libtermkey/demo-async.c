// <poll.h> might need this for sigset_t
#define _XOPEN_SOURCE 600

#include <poll.h>
#include <stdio.h>

#include "termkey.h"

static void on_key(TermKey *tk, TermKeyKey *key)
{
  char buffer[50];
  termkey_strfkey(tk, buffer, sizeof buffer, key, TERMKEY_FORMAT_VIM);
  printf("%s\n", buffer);
}

int main(int argc, char *argv[])
{
  TERMKEY_CHECK_VERSION;

  TermKey *tk = termkey_new(0, 0);

  if(!tk) {
    fprintf(stderr, "Cannot allocate termkey instance\n");
    exit(1);
  }

  struct pollfd fd;

  fd.fd = 0; /* the file descriptor we passed to termkey_new() */
  fd.events = POLLIN;

  TermKeyResult ret;
  TermKeyKey key;

  int running = 1;
  int nextwait = -1;

  while(running) {
    if(poll(&fd, 1, nextwait) == 0) {
      // Timed out
      if(termkey_getkey_force(tk, &key) == TERMKEY_RES_KEY)
        on_key(tk, &key);
    }

    if(fd.revents & (POLLIN|POLLHUP|POLLERR))
      termkey_advisereadable(tk);

    while((ret = termkey_getkey(tk, &key)) == TERMKEY_RES_KEY) {
      on_key(tk, &key);

      if(key.type == TERMKEY_TYPE_UNICODE &&
         key.modifiers & TERMKEY_KEYMOD_CTRL &&
         (key.code.codepoint == 'C' || key.code.codepoint == 'c'))
        running = 0;
    }

    if(ret == TERMKEY_RES_AGAIN)
      nextwait = termkey_get_waittime(tk);
    else
      nextwait = -1;
  }

  termkey_destroy(tk);
}
