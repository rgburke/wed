#include <stdio.h>
#include <glib.h>

#include "termkey.h"

static TermKey *tk;
static int timeout_id;

static void on_key(TermKey *tk, TermKeyKey *key)
{
  char buffer[50];
  termkey_strfkey(tk, buffer, sizeof buffer, key, TERMKEY_FORMAT_VIM);
  printf("%s\n", buffer);
}

static gboolean key_timer(gpointer data)
{
  TermKeyKey key;

  if(termkey_getkey_force(tk, &key) == TERMKEY_RES_KEY)
    on_key(tk, &key);

  return FALSE;
}

static gboolean stdin_io(GIOChannel *source, GIOCondition condition, gpointer data)
{
  if(condition && G_IO_IN) {
    if(timeout_id)
      g_source_remove(timeout_id);

    termkey_advisereadable(tk);

    TermKeyResult ret;
    TermKeyKey key;
    while((ret = termkey_getkey(tk, &key)) == TERMKEY_RES_KEY) {
      on_key(tk, &key);
    }

    if(ret == TERMKEY_RES_AGAIN)
      timeout_id = g_timeout_add(termkey_get_waittime(tk), key_timer, NULL);
  }

  return TRUE;
}

int main(int argc, char *argv[])
{
  TERMKEY_CHECK_VERSION;

  tk = termkey_new(0, 0);

  if(!tk) {
    fprintf(stderr, "Cannot allocate termkey instance\n");
    exit(1);
  }

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  g_io_add_watch(g_io_channel_unix_new(0), G_IO_IN, stdin_io, NULL);

  g_main_loop_run(loop);

  termkey_destroy(tk);
}
