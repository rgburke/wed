CC=cc
CFLAGS=-std=c99 -Wall -Wextra -pedantic -O2 -MMD -MP -DNDEBUG -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
LDFLAGS=-lncursesw -lpcre
LEX=lex
YACC=yacc
AR=ar

SOURCES=wed.c display.c buffer.c util.c input.c session.c status.c command.c file.c value.c list.c hashmap.c config.c encoding.c config_parse_util.c config_parse.c config_scan.c gap_buffer.c buffer_pos.c text_search.c regex_search.c search.c replace.c undo.c file_type.c
OBJECTS=$(SOURCES:.c=.o)
LIBOBJECTS=$(filter-out wed.o, $(OBJECTS))
DEPENDENCIES=$(OBJECTS:.o=.d)

TESTSOURCES=$(wildcard t/*.c)
TESTOBJECTS=$(TESTSOURCES:.c=.t)
TESTS=$(filter-out t/tap.t, $(TESTOBJECTS))
TESTDEPENDENCIES=$(TESTSOURCES:.c=.d)

LIBTERMKEYDIR=lib/libtermkey
LIBTERMKEYLIB=$(LIBTERMKEYDIR)/libtermkey.a
LIBWED=wedlib.a
BINARY=wed

ifeq ($(.DEFAULT_GOAL),)
ifeq ($(DEBUG),1)
.DEFAULT_GOAL := dev
endif
endif

.PHONY: all
all: $(SOURCES) $(BINARY)

.PHONY: dev
dev: CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -g -MMD -MP -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
dev: all test

$(BINARY): $(LIBTERMKEYLIB) $(LIBWED) wed.o
	$(CC) wed.o $(LIBWED) $(LIBTERMKEYLIB) -o $@ $(LDFLAGS)

$(LIBWED): $(OBJECTS)
	$(AR) rcs $(LIBWED) $(LIBOBJECTS)

$(LIBTERMKEYLIB):
	make -C $(LIBTERMKEYDIR)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

-include $(DEPENDENCIES)

config_scan.o: config_scan.c config_parse.c

config_scan.c: config_scan.l
	$(LEX) -o $@ $^

config_parse.c: config_parse.y
	$(YACC) -y -d -o $@ $^

test: $(TESTS)
	@echo Running tests:
	@prove -e ""
	@touch test

-include $(TESTDEPENDENCIES)

t/%.t: t/%.c t/tap.o $(LIBWED) $(LIBTERMKEYLIB)
	$(CC) $(CFLAGS) $< t/tap.o $(LIBWED) $(LIBTERMKEYLIB) -o $@

t/tap.o:
	$(CC) -c $(CFLAGS) t/tap.c -o $@

.PHONY: clean
clean:
	rm -f *.o *.d $(LIBWED) $(BINARY) config_parse.c config_parse.h config_scan.c
	rm -f t/*.o t/*.t t/*.d
	make -C $(LIBTERMKEYDIR) clean
