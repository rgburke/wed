PREFIX?=/usr/local
WEDRUNTIME?=$(PREFIX)/share/wed

CC?=cc
LEX?=lex
YACC?=yacc
AR?=ar

CFLAGS_BASE=-std=c99 -Wall -Wextra -pedantic -MMD -MP \
	    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DWEDRUNTIME=\"$(WEDRUNTIME)\"
CFLAGS=$(CFLAGS_BASE) -O2 -DNDEBUG
CFLAGS_DEBUG=$(CFLAGS_BASE) -Werror -g

LDFLAGS=-lncursesw -lpcre

SOURCES=wed.c display.c buffer.c util.c input.c session.c status.c    \
	command.c file.c value.c list.c hashmap.c config.c encoding.c \
	config_parse_util.c config_parse.c config_scan.c gap_buffer.c \
	buffer_pos.c text_search.c regex_search.c search.c replace.c  \
	undo.c file_type.c regex_util.c
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

