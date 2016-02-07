WED_DEV?=0
PREFIX?=/usr/local
WEDRUNTIME?=$(PREFIX)/share/wed
WED_VERSION_LONG=$(shell git describe --long --tags --dirty --always)
WED_VERSION=$(shell git describe --tags --always)
WED_BUILD_DATETIME=$(shell date '+%Y-%m-%d %H:%M:%S')

CC?=cc
LEX?=lex
YACC?=yacc
AR?=ar

CFLAGS_BASE=-std=c99 -Wall -Wextra -pedantic -MMD -MP \
	    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS=$(CFLAGS_BASE) -O2 -DNDEBUG
CFLAGS_DEBUG=$(CFLAGS_BASE) -Werror -g

LDFLAGS=-lncursesw -lpcre

STATIC_SOURCES=wed.c display.c buffer.c util.c input.c session.c    \
	status.c command.c file.c value.c list.c hashmap.c config.c \
	encoding.c config_parse_util.c gap_buffer.c buffer_pos.c    \
	text_search.c regex_search.c search.c replace.c undo.c      \
	file_type.c regex_util.c syntax.c theme.c prompt.c          \
	prompt_completer.c search_util.c
GENERATED_SOURCES=config_parse.c config_scan.c
SOURCES=$(STATIC_SOURCES) $(GENERATED_SOURCES)

OBJECTS=$(SOURCES:.c=.o)
LIBOBJECTS=$(filter-out wed.o, $(OBJECTS))
DEPENDENCIES=$(OBJECTS:.o=.d)

TESTSOURCES=$(wildcard tests/code/*.c)
TESTOBJECTS=$(TESTSOURCES:.c=.t)
TESTS=$(filter-out tests/code/tap.t, $(TESTOBJECTS))
TESTDEPENDENCIES=$(TESTSOURCES:.c=.d)

LIBTERMKEYDIR=lib/libtermkey
LIBTERMKEYLIB=$(LIBTERMKEYDIR)/libtermkey.a
LIBWED=wedlib.a
BINARY=wed

ifeq ($(MAKECMDGOALS),dev)
WED_DEV=1
else ifeq ($(.DEFAULT_GOAL),)
ifeq ($(WED_DEV),1)
.DEFAULT_GOAL := dev
endif
endif
