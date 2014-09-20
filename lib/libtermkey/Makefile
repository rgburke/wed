ifeq ($(shell uname),Darwin)
  LIBTOOL ?= glibtool
else
  LIBTOOL ?= libtool
endif

ifneq ($(VERBOSE),1)
  LIBTOOL +=--quiet
endif

CFLAGS +=-Wall -std=c99

ifeq ($(DEBUG),1)
  CFLAGS +=-ggdb -DDEBUG
endif

ifeq ($(PROFILE),1)
  CFLAGS +=-pg
  LDFLAGS+=-pg
endif

ifeq ($(shell pkg-config --atleast-version=0.1.0 unibilium && echo 1),1)
  CFLAGS +=$(shell pkg-config --cflags unibilium) -DHAVE_UNIBILIUM
  LDFLAGS+=$(shell pkg-config --libs   unibilium)
else ifeq ($(shell pkg-config ncursesw && echo 1),1)
  CFLAGS +=$(shell pkg-config --cflags ncursesw)
  LDFLAGS+=$(shell pkg-config --libs   ncursesw)
else
  LDFLAGS+=-lncurses
endif

OBJECTS=termkey.lo driver-csi.lo driver-ti.lo
LIBRARY=libtermkey.la

DEMOS=demo demo-async

ifeq ($(shell pkg-config glib-2.0 && echo 1),1)
  DEMOS+=demo-glib
endif

DEMO_OBJECTS=$(DEMOS:=.lo)

TESTSOURCES=$(wildcard t/[0-9]*.c)
TESTFILES=$(TESTSOURCES:.c=.t)

VERSION_MAJOR=0
VERSION_MINOR=17

VERSION_CURRENT=12
VERSION_REVISION=0
VERSION_AGE=11

PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include
MANDIR=$(PREFIX)/share/man
MAN3DIR=$(MANDIR)/man3
MAN7DIR=$(MANDIR)/man7

all: $(LIBRARY) $(DEMOS)

%.lo: %.c termkey.h termkey-internal.h
	$(LIBTOOL) --mode=compile --tag=CC $(CC) $(CFLAGS) -o $@ -c $<

$(LIBRARY): $(OBJECTS)
	$(LIBTOOL) --mode=link --tag=CC $(CC) -rpath $(LIBDIR) -version-info $(VERSION_CURRENT):$(VERSION_REVISION):$(VERSION_AGE) $(LDFLAGS) -o $@ $^

demo: $(LIBRARY) demo.lo
	$(LIBTOOL) --mode=link --tag=CC $(CC) -o $@ $^

demo-async: $(LIBRARY) demo-async.lo
	$(LIBTOOL) --mode=link --tag=CC $(CC) -o $@ $^

demo-glib.lo: demo-glib.c termkey.h
	$(LIBTOOL) --mode=compile --tag=CC $(CC) -o $@ -c $< $(shell pkg-config glib-2.0 --cflags)

demo-glib: $(LIBRARY) demo-glib.lo
	$(LIBTOOL) --mode=link --tag=CC $(CC) -o $@ $^ $(shell pkg-config glib-2.0 --libs)

t/%.t: t/%.c $(LIBRARY) t/taplib.lo
	$(LIBTOOL) --mode=link --tag=CC $(CC) -o $@ $^

t/taplib.lo: t/taplib.c
	$(LIBTOOL) --mode=compile --tag=CC $(CC) $(CFLAGS) -o $@ -c $^

.PHONY: test
test: $(TESTFILES)
	prove -e ""

.PHONY: clean-test
clean-test:
	$(LIBTOOL) --mode=clean rm -f $(TESTFILES) t/taplib.lo

.PHONY: clean
clean: clean-test
	$(LIBTOOL) --mode=clean rm -f $(OBJECTS) $(DEMO_OBJECTS)
	$(LIBTOOL) --mode=clean rm -f $(LIBRARY)
	$(LIBTOOL) --mode=clean rm -rf $(DEMOS)

.PHONY: install
install: install-inc install-lib install-man
	$(LIBTOOL) --mode=finish $(DESTDIR)$(LIBDIR)

install-inc: termkey.h
	install -d $(DESTDIR)$(INCDIR)
	install -m644 termkey.h $(DESTDIR)$(INCDIR)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	sed "s,@LIBDIR@,$(LIBDIR),;s,@INCDIR@,$(INCDIR)," <termkey.pc.in >$(DESTDIR)$(LIBDIR)/pkgconfig/termkey.pc

install-lib: $(LIBRARY)
	install -d $(DESTDIR)$(LIBDIR)
	$(LIBTOOL) --mode=install install libtermkey.la $(DESTDIR)$(LIBDIR)/libtermkey.la

install-man:
	install -d $(DESTDIR)$(MAN3DIR)
	install -d $(DESTDIR)$(MAN7DIR)
	for F in man/*.3; do \
	  gzip <$$F >$(DESTDIR)$(MAN3DIR)/$${F#man/}.gz; \
	done
	for F in man/*.7; do \
	  gzip <$$F >$(DESTDIR)$(MAN7DIR)/$${F#man/}.gz; \
	done
	while read FROM EQ TO; do \
	  echo ln -sf $$TO.gz $(DESTDIR)$(MAN3DIR)/$$FROM.gz; \
	done < man/also

