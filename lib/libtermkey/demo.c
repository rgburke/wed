// we want optarg
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "termkey.h"

int main(int argc, char *argv[])
{
  TERMKEY_CHECK_VERSION;

  int mouse = 0;
  int mouse_proto = 0;
  TermKeyFormat format = TERMKEY_FORMAT_VIM;

  char buffer[50];
  TermKey *tk;

  int opt;
  while((opt = getopt(argc, argv, "m::p:")) != -1) {
    switch(opt) {
    case 'm':
      if(optarg)
        mouse = atoi(optarg);
      else
        mouse = 1000;

      break;

    case 'p':
      mouse_proto = atoi(optarg);
      break;

    default:
      fprintf(stderr, "Usage: %s [-m]\n", argv[0]);
      return 1;
    }
  }

  tk = termkey_new(0, TERMKEY_FLAG_SPACESYMBOL|TERMKEY_FLAG_CTRLC);

  if(!tk) {
    fprintf(stderr, "Cannot allocate termkey instance\n");
    exit(1);
  }

  if(termkey_get_flags(tk) & TERMKEY_FLAG_UTF8)
    printf("Termkey in UTF-8 mode\n");
  else if(termkey_get_flags(tk) & TERMKEY_FLAG_RAW)
    printf("Termkey in RAW mode\n");

  TermKeyResult ret;
  TermKeyKey key;

  if(mouse) {
    printf("\033[?%dhMouse mode active\n", mouse);
    if(mouse_proto)
      printf("\033[?%dh", mouse_proto);
  }

  while((ret = termkey_waitkey(tk, &key)) != TERMKEY_RES_EOF) {
    if(ret == TERMKEY_RES_KEY) {
      termkey_strfkey(tk, buffer, sizeof buffer, &key, format);
      if(key.type == TERMKEY_TYPE_MOUSE) {
        int line, col;
        termkey_interpret_mouse(tk, &key, NULL, NULL, &line, &col);
        printf("%s at line=%d, col=%d\n", buffer, line, col);
      }
      else if(key.type == TERMKEY_TYPE_POSITION) {
        int line, col;
        termkey_interpret_position(tk, &key, &line, &col);
        printf("Cursor position report at line=%d, col=%d\n", line, col);
      }
      else if(key.type == TERMKEY_TYPE_MODEREPORT) {
        int initial, mode, value;
        termkey_interpret_modereport(tk, &key, &initial, &mode, &value);
        printf("Mode report %s mode %d = %d\n", initial ? "DEC" : "ANSI", mode, value);
      }
      else if(key.type == TERMKEY_TYPE_UNKNOWN_CSI) {
        long args[16];
        size_t nargs = 16;
        unsigned long command;
        termkey_interpret_csi(tk, &key, args, &nargs, &command);
        printf("Unrecognised CSI %c %ld;%ld %c%c\n", (char)(command >> 8), args[0], args[1], (char)(command >> 16), (char)command);
      }
      else {
        printf("Key %s\n", buffer);
      }

      if(key.type == TERMKEY_TYPE_UNICODE &&
         key.modifiers & TERMKEY_KEYMOD_CTRL &&
         (key.code.codepoint == 'C' || key.code.codepoint == 'c'))
        break;

      if(key.type == TERMKEY_TYPE_UNICODE &&
         key.modifiers == 0 &&
         key.code.codepoint == '?') {
        // printf("\033[?6n"); // DECDSR 6 == request cursor position
        printf("\033[?1$p"); // DECRQM == request mode, DEC origin mode
        fflush(stdout);
      }
    }
    else if(ret == TERMKEY_RES_ERROR) {
      if(errno != EINTR) {
        perror("termkey_waitkey");
        break;
      }
      printf("Interrupted by signal\n");
    }
  }

  if(mouse)
    printf("\033[?%dlMouse mode deactivated\n", mouse);

  termkey_destroy(tk);
}
