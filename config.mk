WED_DEV?=0
PREFIX?=/usr/local
WEDRUNTIME?=$(PREFIX)/share/wed
WED_VERSION_DEFAULT=v0.1
WED_VERSION_LONG=$(shell git describe --long --tags --dirty --always \
		   2>/dev/null || echo '$(WED_VERSION_DEFAULT)')
WED_VERSION=$(shell git describe --tags --always 2>/dev/null || \
	      echo '$(WED_VERSION_DEFAULT)')
WED_BUILD_DATETIME=$(shell date '+%Y-%m-%d %H:%M:%S')

CC?=cc
FLEX?=flex
BISON?=bison
AR?=ar

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

CFLAGS=-std=c99 -Wall -Wextra -pedantic -MMD -MP
LDFLAGS=-lncursesw -lpcre

OS=$(shell uname)

ifeq ($(OS),Linux)
	CFLAGS+=-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
else ifeq ($(OS),FreeBSD)
	CFLAGS+=-I/usr/local/include
	LDFLAGS+=-L/usr/local/lib
else ifeq ($(findstring CYGWIN,$(OS)),CYGWIN)
	CFLAGS+=-U__STRICT_ANSI__
endif

ifeq ($(WED_DEV),1)
	CFLAGS+=-Werror -g
else
	CFLAGS+=-O2 -DNDEBUG
endif
