#include "termkey.h"
#include "termkey-internal.h"

static void *new_driver(termkey_t *tk, const char *term)
{
  return NULL;
}

static int start_driver(termkey_t *tk, void *info)
{
  // This is optional
  return 1;
}

static int stop_driver(termkey_t *tk, void *info)
{
  // This is optional
  return 1;
}

static void free_driver(void *info)
{
}

static termkey_result getkey(termkey_t *tk, void *info, termkey_key *key, int force)
{
  return TERMKEY_RES_NONE;
}

struct termkey_driver termkey_driver_null = {
  .new_driver  = new_driver,
  .free_driver = free_driver,

  .start_driver = start_driver,
  .stop_driver  = stop_driver,

  .getkey = getkey,
};
