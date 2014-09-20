// we want strdup()
#define _XOPEN_SOURCE 600

#include "termkey.h"
#include "termkey-internal.h"

#ifdef HAVE_UNIBILIUM
# include <unibilium.h>
#else
# include <curses.h>
# include <term.h>

/* curses.h has just poluted our namespace. We want this back */
# undef buttons
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/* To be efficient at lookups, we store the byte sequence => keyinfo mapping
 * in a trie. This avoids a slow linear search through a flat list of
 * sequences. Because it is likely most nodes will be very sparse, we optimise
 * vector to store an extent map after the database is loaded.
 */

typedef enum {
  TYPE_KEY,
  TYPE_ARR,
  TYPE_MOUSE,
} trie_nodetype;

struct trie_node {
  trie_nodetype type;
};

struct trie_node_key {
  trie_nodetype type;
  struct keyinfo key;
};

struct trie_node_arr {
  trie_nodetype type;
  unsigned char min, max; /* INCLUSIVE endpoints of the extent range */
  struct trie_node *arr[]; /* dynamic size at allocation time */
};

typedef struct {
  TermKey *tk;

  struct trie_node *root;

  char *start_string;
  char *stop_string;
} TermKeyTI;

static int funcname2keysym(const char *funcname, TermKeyType *typep, TermKeySym *symp, int *modmask, int *modsetp);
static int insert_seq(TermKeyTI *ti, const char *seq, struct trie_node *node);

static struct trie_node *new_node_key(TermKeyType type, TermKeySym sym, int modmask, int modset)
{
  struct trie_node_key *n = malloc(sizeof(*n));
  if(!n)
    return NULL;

  n->type = TYPE_KEY;

  n->key.type = type;
  n->key.sym  = sym;
  n->key.modifier_mask = modmask;
  n->key.modifier_set  = modset;

  return (struct trie_node*)n;
}

static struct trie_node *new_node_arr(unsigned char min, unsigned char max)
{
  struct trie_node_arr *n = malloc(sizeof(*n) + ((int)max-min+1) * sizeof(n->arr[0]));
  if(!n)
    return NULL;

  n->type = TYPE_ARR;
  n->min = min; n->max = max;

  int i;
  for(i = min; i <= max; i++)
    n->arr[i-min] = NULL;

  return (struct trie_node*)n;
}

static struct trie_node *lookup_next(struct trie_node *n, unsigned char b)
{
  switch(n->type) {
  case TYPE_KEY:
  case TYPE_MOUSE:
    fprintf(stderr, "ABORT: lookup_next within a TYPE_KEY node\n");
    abort();
  case TYPE_ARR:
    {
      struct trie_node_arr *nar = (struct trie_node_arr*)n;
      if(b < nar->min || b > nar->max)
        return NULL;
      return nar->arr[b - nar->min];
    }
  }

  return NULL; // Never reached but keeps compiler happy
}

static void free_trie(struct trie_node *n)
{
  switch(n->type) {
  case TYPE_KEY:
  case TYPE_MOUSE:
    break;
  case TYPE_ARR:
    {
      struct trie_node_arr *nar = (struct trie_node_arr*)n;
      int i;
      for(i = nar->min; i <= nar->max; i++)
        if(nar->arr[i - nar->min])
          free_trie(nar->arr[i - nar->min]);
      break;
    }
  }

  free(n);
}

static struct trie_node *compress_trie(struct trie_node *n)
{
  if(!n)
    return NULL;

  switch(n->type) {
  case TYPE_KEY:
  case TYPE_MOUSE:
    return n;
  case TYPE_ARR:
    {
      struct trie_node_arr *nar = (struct trie_node_arr*)n;
      unsigned char min, max;
      // Find the real bounds
      for(min = 0; !nar->arr[min]; min++)
        ;
      for(max = 0xff; !nar->arr[max]; max--)
        ;

      struct trie_node_arr *new = (struct trie_node_arr*)new_node_arr(min, max);
      int i;
      for(i = min; i <= max; i++)
        new->arr[i - min] = compress_trie(nar->arr[i]);

      free(nar);
      return (struct trie_node*)new;
    }
  }

  return n;
}

static int load_terminfo(TermKeyTI *ti, const char *term)
{
  int i;

#ifdef HAVE_UNIBILIUM
  unibi_term *unibi = unibi_from_term(term);
  if(!unibi)
    return 0;
#else
  int err;

  /* Have to cast away the const. But it's OK - we know terminfo won't really
   * modify term */
  if(setupterm((char*)term, 1, &err) != OK)
    return 0;
#endif

#ifdef HAVE_UNIBILIUM
  for(i = unibi_string_begin_+1; i < unibi_string_end_; i++)
#else
  for(i = 0; strfnames[i]; i++)
#endif
  {
    // Only care about the key_* constants
#ifdef HAVE_UNIBILIUM
    const char *name = unibi_name_str(i);
#else
    const char *name = strfnames[i];
#endif
    if(strncmp(name, "key_", 4) != 0)
      continue;

#ifdef HAVE_UNIBILIUM
    const char *value = unibi_get_str(unibi, i);
#else
    const char *value = tigetstr(strnames[i]);
#endif
    if(!value || value == (char*)-1)
      continue;

    struct trie_node *node = NULL;

    if(strcmp(name + 4, "mouse") == 0) {
      node = malloc(sizeof(*node));
      if(!node)
        return 0;

      node->type = TYPE_MOUSE;
    }
    else {
      TermKeyType type;
      TermKeySym sym;
      int mask = 0;
      int set  = 0;

      if(!funcname2keysym(name + 4, &type, &sym, &mask, &set))
        continue;

      if(sym == TERMKEY_SYM_NONE)
        continue;

      node = new_node_key(type, sym, mask, set);
    }

    if(node)
      if(!insert_seq(ti, value, node)) {
        free(node);
        return 0;
      }
  }

  /* Take copies of these terminfo strings, in case we build multiple termkey
   * instances for multiple different termtypes, and it's different by the
   * time we want to use it
   */
#ifdef HAVE_UNIBILIUM
  const char *keypad_xmit = unibi_get_str(unibi, unibi_pkey_xmit);
#endif

  if(keypad_xmit)
    ti->start_string = strdup(keypad_xmit);
  else
    ti->start_string = NULL;

#ifdef HAVE_UNIBILIUM
  const char *keypad_local = unibi_get_str(unibi, unibi_pkey_local);
#endif

  if(keypad_local)
    ti->stop_string = strdup(keypad_local);
  else
    ti->stop_string = NULL;

#ifdef HAVE_UNIBILIUM
  unibi_destroy(unibi);
#endif

  return 1;
}

static void *new_driver(TermKey *tk, const char *term)
{
  TermKeyTI *ti = malloc(sizeof *ti);
  if(!ti)
    return NULL;

  ti->tk = tk;

  ti->root = new_node_arr(0, 0xff);
  if(!ti->root)
    goto abort_free_ti;

  if(!load_terminfo(ti, term))
    goto abort_free_trie;

  ti->root = compress_trie(ti->root);

  return ti;

abort_free_trie:
  free_trie(ti->root);

abort_free_ti:
  free(ti);

  return NULL;
}

static int start_driver(TermKey *tk, void *info)
{
  TermKeyTI *ti = info;
  struct stat statbuf;
  char *start_string = ti->start_string;
  size_t len;

  if(tk->fd == -1 || !start_string)
    return 1;

  /* The terminfo database will contain keys in application cursor key mode.
   * We may need to enable that mode
   */

  /* There's no point trying to write() to a pipe */
  if(fstat(tk->fd, &statbuf) == -1)
    return 0;

  if(S_ISFIFO(statbuf.st_mode))
    return 1;

  // Can't call putp or tputs because they suck and don't give us fd control
  len = strlen(start_string);
  while(len) {
    size_t written = write(tk->fd, start_string, len);
    if(written == -1)
      return 0;
    start_string += written;
    len -= written;
  }
  return 1;
}

static int stop_driver(TermKey *tk, void *info)
{
  TermKeyTI *ti = info;
  struct stat statbuf;
  char *stop_string = ti->stop_string;
  size_t len;

  if(tk->fd == -1 || !stop_string)
    return 1;

  /* There's no point trying to write() to a pipe */
  if(fstat(tk->fd, &statbuf) == -1)
    return 0;

  if(S_ISFIFO(statbuf.st_mode))
    return 1;

  /* The terminfo database will contain keys in application cursor key mode.
   * We may need to enable that mode
   */

  // Can't call putp or tputs because they suck and don't give us fd control
  len = strlen(stop_string);
  while(len) {
    size_t written = write(tk->fd, stop_string, len);
    if(written == -1)
      return 0;
    stop_string += written;
    len -= written;
  }
  return 1;
}

static void free_driver(void *info)
{
  TermKeyTI *ti = info;

  free_trie(ti->root);

  if(ti->start_string)
    free(ti->start_string);

  if(ti->stop_string)
    free(ti->stop_string);

  free(ti);
}

#define CHARAT(i) (tk->buffer[tk->buffstart + (i)])

static TermKeyResult peekkey(TermKey *tk, void *info, TermKeyKey *key, int force, size_t *nbytep)
{
  TermKeyTI *ti = info;

  if(tk->buffcount == 0)
    return tk->is_closed ? TERMKEY_RES_EOF : TERMKEY_RES_NONE;

  struct trie_node *p = ti->root;

  unsigned int pos = 0;
  while(pos < tk->buffcount) {
    p = lookup_next(p, CHARAT(pos));
    if(!p)
      break;

    pos++;

    if(p->type == TYPE_KEY) {
      struct trie_node_key *nk = (struct trie_node_key*)p;
      key->type      = nk->key.type;
      key->code.sym  = nk->key.sym;
      key->modifiers = nk->key.modifier_set;
      *nbytep = pos;
      return TERMKEY_RES_KEY;
    }
    else if(p->type == TYPE_MOUSE) {
      tk->buffstart += pos;
      tk->buffcount -= pos;

      TermKeyResult mouse_result = (*tk->method.peekkey_mouse)(tk, key, nbytep);

      tk->buffstart -= pos;
      tk->buffcount += pos;

      if(mouse_result == TERMKEY_RES_KEY)
        *nbytep += pos;

      return mouse_result;
    }
  }

  // If p is not NULL then we hadn't walked off the end yet, so we have a
  // partial match
  if(p && !force)
    return TERMKEY_RES_AGAIN;

  return TERMKEY_RES_NONE;
}

static struct {
  const char *funcname;
  TermKeyType type;
  TermKeySym sym;
  int mods;
} funcs[] =
{
  /* THIS LIST MUST REMAIN SORTED! */
  { "backspace", TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_BACKSPACE, 0 },
  { "begin",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_BEGIN,     0 },
  { "beg",       TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_BEGIN,     0 },
  { "btab",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_TAB,       TERMKEY_KEYMOD_SHIFT },
  { "cancel",    TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_CANCEL,    0 },
  { "clear",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_CLEAR,     0 },
  { "close",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_CLOSE,     0 },
  { "command",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_COMMAND,   0 },
  { "copy",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_COPY,      0 },
  { "dc",        TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_DELETE,    0 },
  { "down",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_DOWN,      0 },
  { "end",       TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_END,       0 },
  { "enter",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_ENTER,     0 },
  { "exit",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_EXIT,      0 },
  { "find",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_FIND,      0 },
  { "help",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_HELP,      0 },
  { "home",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_HOME,      0 },
  { "ic",        TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_INSERT,    0 },
  { "left",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_LEFT,      0 },
  { "mark",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_MARK,      0 },
  { "message",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_MESSAGE,   0 },
  { "mouse",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_NONE,      0 },
  { "move",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_MOVE,      0 },
  { "next",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_PAGEDOWN,  0 }, // Not quite, but it's the best we can do
  { "npage",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_PAGEDOWN,  0 },
  { "open",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_OPEN,      0 },
  { "options",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_OPTIONS,   0 },
  { "ppage",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_PAGEUP,    0 },
  { "previous",  TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_PAGEUP,    0 }, // Not quite, but it's the best we can do
  { "print",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_PRINT,     0 },
  { "redo",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_REDO,      0 },
  { "reference", TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_REFERENCE, 0 },
  { "refresh",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_REFRESH,   0 },
  { "replace",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_REPLACE,   0 },
  { "restart",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_RESTART,   0 },
  { "resume",    TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_RESUME,    0 },
  { "right",     TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_RIGHT,     0 },
  { "save",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_SAVE,      0 },
  { "select",    TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_SELECT,    0 },
  { "suspend",   TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_SUSPEND,   0 },
  { "undo",      TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_UNDO,      0 },
  { "up",        TERMKEY_TYPE_KEYSYM, TERMKEY_SYM_UP,        0 },
  { NULL },
};

static int funcname2keysym(const char *funcname, TermKeyType *typep, TermKeySym *symp, int *modmaskp, int *modsetp)
{
  // Binary search

  int start = 0;
  int end   = sizeof(funcs)/sizeof(funcs[0]); // is "one past" the end of the range

  while(1) {
    int i = (start+end) / 2;
    int cmp = strcmp(funcname, funcs[i].funcname);

    if(cmp == 0) {
      *typep    = funcs[i].type;
      *symp     = funcs[i].sym;
      *modmaskp = funcs[i].mods;
      *modsetp  = funcs[i].mods;
      return 1;
    }
    else if(end == start + 1)
      // That was our last choice and it wasn't it - not found
      break;
    else if(cmp > 0)
      start = i;
    else
      end = i;
  }

  if(funcname[0] == 'f' && isdigit(funcname[1])) {
    *typep = TERMKEY_TYPE_FUNCTION;
    *symp  = atoi(funcname + 1);
    return 1;
  }

  // Last-ditch attempt; maybe it's a shift key?
  if(funcname[0] == 's' && funcname2keysym(funcname + 1, typep, symp, modmaskp, modsetp)) {
    *modmaskp |= TERMKEY_KEYMOD_SHIFT;
    *modsetp  |= TERMKEY_KEYMOD_SHIFT;
    return 1;
  }

#ifdef DEBUG
  fprintf(stderr, "TODO: Need to convert funcname %s to a type/sym\n", funcname);
#endif

  return 0;
}

static int insert_seq(TermKeyTI *ti, const char *seq, struct trie_node *node)
{
  int pos = 0;
  struct trie_node *p = ti->root;

  // Unsigned because we'll be using it as an array subscript
  unsigned char b;

  while((b = seq[pos])) {
    struct trie_node *next = lookup_next(p, b);
    if(!next)
      break;
    p = next;
    pos++;
  }

  while((b = seq[pos])) {
    struct trie_node *next;
    if(seq[pos+1])
      // Intermediate node
      next = new_node_arr(0, 0xff);
    else
      // Final key node
      next = node;

    if(!next)
      return 0;

    switch(p->type) {
    case TYPE_ARR:
      {
        struct trie_node_arr *nar = (struct trie_node_arr*)p;
        if(b < nar->min || b > nar->max) {
          fprintf(stderr, "ASSERT FAIL: Trie insert at 0x%02x is outside of extent bounds (0x%02x..0x%02x)\n",
              b, nar->min, nar->max);
          abort();
        }
        nar->arr[b - nar->min] = next;
        p = next;
        break;
      }
    case TYPE_KEY:
    case TYPE_MOUSE:
      fprintf(stderr, "ASSERT FAIL: Tried to insert child node in TYPE_KEY\n");
      abort();
    }

    pos++;
  }

  return 1;
}

struct TermKeyDriver termkey_driver_ti = {
  .name        = "terminfo",

  .new_driver  = new_driver,
  .free_driver = free_driver,

  .start_driver = start_driver,
  .stop_driver  = stop_driver,

  .peekkey = peekkey,
};
