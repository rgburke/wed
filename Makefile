include config.mk

.PHONY: all
all: $(SOURCES) $(BINARY)

.PHONY: dev
dev: CFLAGS=$(CFLAGS_DEBUG)
dev: all test

$(BINARY): $(LIBTERMKEYLIB) $(LIBWED) wed.o
	$(CC) wed.o $(LIBWED) $(LIBTERMKEYLIB) -o $@ $(LDFLAGS)

$(LIBWED): $(OBJECTS)
	$(AR) rcs $(LIBWED) $(LIBOBJECTS)

$(LIBTERMKEYLIB):
	$(MAKE) -C $(LIBTERMKEYDIR)

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
	$(MAKE) -C $(LIBTERMKEYDIR) clean

.PHONY: install
install:
	install -d $(PREFIX)/bin
	install -m755 -s $(BINARY) $(PREFIX)/bin
	install -m755 -d $(WEDRUNTIME)	
	cp -fpr --no-preserve=ownership wedruntime/* $(WEDRUNTIME)
	find $(WEDRUNTIME) -type f -exec chmod 644 {} \;

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(BINARY)
	rm -fr $(WEDRUNTIME)
