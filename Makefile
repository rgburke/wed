include config.mk

WED_VERSION_DEFAULT=v0.1
WED_VERSION_LONG=$(shell git describe --long --tags --dirty --always \
				   2>/dev/null || echo '$(WED_VERSION_DEFAULT)')
WED_VERSION=$(shell git describe --tags --abbrev=0 2>/dev/null || \
			  echo '$(WED_VERSION_DEFAULT)')
WED_BUILD_DATETIME=$(shell date '+%Y-%m-%d %H:%M:%S %Z')

STATIC_SOURCES=wed.c buffer.c util.c input.c session.c \
	status.c command.c file.c value.c list.c hashmap.c config.c  \
	encoding.c config_parse_util.c gap_buffer.c buffer_pos.c     \
	text_search.c regex_search.c search.c replace.c undo.c       \
	file_type.c regex_util.c syntax.c theme.c prompt.c           \
	prompt_completer.c search_util.c external_command.c          \
	clipboard.c radix_tree.c buffer_view.c tui.c tabbed_view.c   \
	syntax_manager.c wed_syntax.c
GENERATED_SOURCES=config_parse.c config_scan.c
GNU_SOURCE_HIGHLIGHT_SOURCES=gnu_source_highlight_syntax.c
GNU_SOURCE_HIGHLIGHT_CXX_SOURCES=gnu_source_highlight.cc
LUA_SOURCES=wed_lua.c scintillua_syntax.c

SOURCES:=$(STATIC_SOURCES) $(GENERATED_SOURCES)
OBJECTS:=$(SOURCES:.c=.o)

ifeq ($(WED_FEATURE_LUA),1)
	SOURCES:=$(SOURCES) $(LUA_SOURCES)
	OBJECTS:=$(OBJECTS) $(LUA_SOURCES:.c=.o)
endif

ifeq ($(WED_FEATURE_GNU_SOURCE_HIGHLIGHT),1)
	SOURCES:=$(SOURCES) $(GNU_SOURCE_HIGHLIGHT_SOURCES) \
		$(GNU_SOURCE_HIGHLIGHT_CXX_SOURCES)
	OBJECTS:=$(OBJECTS) $(GNU_SOURCE_HIGHLIGHT_SOURCES:.c=.o) \
		$(GNU_SOURCE_HIGHLIGHT_CXX_SOURCES:.cc=.o)
endif

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

.PHONY: all
all: $(BINARY)

.PHONY: dev
dev: CFLAGS+=-g -ggdb -Wall -Wextra -pedantic -MMD -MP -O0 -UNDEBUG
dev: CXXFLAGS+=-g -ggdb -Wall -Wextra -pedantic -MMD -MP -O0 -UNDEBUG -Wno-variadic-macros
dev: all test

$(BINARY): $(SOURCES) $(LIBTERMKEYLIB) $(LIBWED) wed.o
	$(CC) wed.o $(LIBWED) $(LIBTERMKEYLIB) -o $@ $(LDFLAGS)

$(LIBWED): $(OBJECTS)
	$(AR) rcs $(LIBWED) $(LIBOBJECTS)

$(LIBTERMKEYLIB):
	$(MAKE) -C $(LIBTERMKEYDIR)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.cc.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

-include $(DEPENDENCIES)

config_scan.o: config_scan.c config_parse.c

config_scan.c: config_scan.l
	$(FLEX) -o $@ $<

config_parse.c: config_parse.y
	$(BISON) -y -d -o $@ $<

$(SOURCES): build_config.h

build_config.h: config.mk
	@echo '/* Autogenerated by Makefile */' > build_config.h
	@echo '#ifndef WED_BUILD_CONFIG_H' >> build_config.h
	@echo '#define WED_BUILD_CONFIG_H' >> build_config.h
	@echo '#define WEDRUNTIME "$(DESTDIR)$(WEDRUNTIME)"' >> build_config.h
	@echo '#define WED_VERSION "$(WED_VERSION)"' >> build_config.h
	@echo '#define WED_VERSION_LONG "$(WED_VERSION_LONG)"' >> build_config.h
	@echo '#define WED_BUILD_DATETIME "$(WED_BUILD_DATETIME)"' >> build_config.h
	@echo '#define WED_PCRE_VERSION_GE_8_20 $(WED_PCRE_VERSION_GE_8_20)' >> build_config.h
	@echo '#define WED_FEATURE_LUA $(WED_FEATURE_LUA)' >> build_config.h
	@echo '#define WED_FEATURE_GNU_SOURCE_HIGHLIGHT $(WED_FEATURE_GNU_SOURCE_HIGHLIGHT)' >> build_config.h
	@echo '#define WED_DEFAULT_SDT "$(WED_DEFAULT_SDT)"' >> build_config.h
	@echo '#endif' >> build_config.h

test: $(BINARY) $(TESTS)
	@echo 'Running code tests:'
	@prove -e '' tests/code
	@echo 'Running text tests:'
	@tests/text/run_text_tests.sh
	@touch test

-include $(TESTDEPENDENCIES)

tests/code/%.t: tests/code/%.c tests/code/tap.o $(LIBWED) $(LIBTERMKEYLIB)
	$(CC) $(CFLAGS) $< tests/code/tap.o $(LIBWED) $(LIBTERMKEYLIB) -o $@

tests/code/tap.o:
	$(CC) -c $(CFLAGS) tests/code/tap.c -o $@

.PHONY: clean
clean:
	rm -f *.o *.d $(LIBWED) $(BINARY) config_parse.c config_parse.h config_scan.c build_config.h
	rm -f tests/code/*.o tests/code/*.t tests/code/*.d
	$(MAKE) -C $(LIBTERMKEYDIR) clean

.PHONY: install
install:
	@echo 'Installing wed under $(DESTDIR)$(PREFIX)'
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install -m755 -s $(BINARY) $(DESTDIR)$(PREFIX)/bin
	@install -m755 '$(WEDCLIPBOARD)' $(DESTDIR)$(PREFIX)/bin
	@install -m755 -d $(DESTDIR)$(WEDRUNTIME)	
	@cp -fr wedruntime/* $(DESTDIR)$(WEDRUNTIME)
	@find $(DESTDIR)$(WEDRUNTIME) -type f -exec chmod 644 {} \;
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	@sed 's/VERSION/$(WED_VERSION)/g' doc/man/wed.1 | gzip > $(DESTDIR)$(PREFIX)/share/man/man1/wed.1.gz
	@chmod 644 $(DESTDIR)$(PREFIX)/share/man/man1/wed.1.gz

.PHONY: uninstall
uninstall:
	@echo 'Uninstalling wed'
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	@rm -f '$(DESTDIR)$(PREFIX)/bin/$(WEDCLIPBOARD)'
	@rm -fr $(DESTDIR)$(WEDRUNTIME)
	@rm -f $(DESTDIR)$(PREFIX)/share/man/man1/wed.1.gz
