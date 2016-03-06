WED_DEV?=0
PREFIX?=/usr/local
WEDRUNTIME?=$(PREFIX)/share/wed
WED_VERSION_DEFAULT=v0.1
WED_VERSION_LONG=$(shell git describe --long --tags --dirty --always \
		   2>/dev/null || echo '$(WED_VERSION_DEFAULT)')
WED_VERSION=$(shell git describe --tags --abbrev=0 2>/dev/null || \
	      echo '$(WED_VERSION_DEFAULT)')
WED_BUILD_DATETIME=$(shell date '+%Y-%m-%d %H:%M:%S')

# Only versions of PCRE >= 8.20 have pcre_free_study
WED_PCRE_VERSION_GE_8_20=$(shell pcre-config --version 2>/dev/null | awk -F'.' \
	'BEGIN { output = 0; } \
	 NR == 1 && ($$1 > 8 || ($$1 == 8 && $$2 >= 20)) { output = 1; } \
	 END { print output; }' 2>/dev/null)

CC?=cc
FLEX?=flex
BISON?=bison
AR?=ar

STATIC_SOURCES=wed.c display.c buffer.c util.c input.c session.c    \
	status.c command.c file.c value.c list.c hashmap.c config.c \
	encoding.c config_parse_util.c gap_buffer.c buffer_pos.c    \
	text_search.c regex_search.c search.c replace.c undo.c      \
	file_type.c regex_util.c syntax.c theme.c prompt.c          \
	prompt_completer.c search_util.c external_command.c         \
	clipboard.c
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
WEDCLIPBOARD=wed-clipboard

ifeq ($(MAKECMDGOALS),dev)
	WED_DEV=1
else ifeq ($(.DEFAULT_GOAL),)
	ifeq ($(WED_DEV),1)
		.DEFAULT_GOAL := dev
	endif
endif

CFLAGS=-std=c99 -Wall -Wextra -pedantic -MMD -MP
LDFLAGS=-lncursesw -lpcre -lrt

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
