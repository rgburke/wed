CC=cc
CFLAGS=-c -std=c99 -Wall -Wextra -pedantic -O2 -DNDEBUG -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
LDFLAGS=-lncursesw
LEX=lex
YACC=yacc
SOURCES=wed.c display.c buffer.c util.c input.c session.c status.c command.c file.c value.c list.c hashmap.c config.c encoding.c config_parse_util.c config_parse.c config_scan.c gap_buffer.c buffer_pos.c
LIBTERMKEYDIR=lib/libtermkey
LIBTERMKEYLIB=libtermkey.a
OBJECTS=$(SOURCES:.c=.o)
BINARY=wed

ifeq ($(.DEFAULT_GOAL),)
ifeq ($(DEBUG),1)
.DEFAULT_GOAL := dev
endif
endif

all: $(SOURCES) libtermkey $(BINARY)

dev: CFLAGS=-c -std=c99 -Wall -Wextra -Werror -pedantic -g -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
dev: all

$(BINARY): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBTERMKEYDIR)/$(LIBTERMKEYLIB) -o $@ $(LDFLAGS)

libtermkey:
	make -C $(LIBTERMKEYDIR)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

config_scan.o: config_scan.c config_parse.c

config_scan.c: config_scan.l
	$(LEX) -o $@ $^

config_parse.c: config_parse.y
	$(YACC) -y -d -o $@ $^

clean:
	rm -rf *.o $(BINARY) config_parse.c config_parse.h config_scan.c
	make -C $(LIBTERMKEYDIR) clean

