CC=cc
CFLAGS=-c -std=c99 -Wall -Wextra -Werror -pedantic -g -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
#CFLAGS=-c -std=c99 -O2 -Wall -Wextra -pedantic
LDFLAGS=-lncursesw
SOURCES=wed.c display.c buffer.c util.c input.c session.c status.c command.c file.c variable.c list.c hashmap.c config.c encoding.c
LIBTERMKEYDIR=lib/libtermkey
LIBTERMKEYLIB=libtermkey.a
OBJECTS=$(SOURCES:.c=.o)
BINARY=wed

all: $(SOURCES) libtermkey $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBTERMKEYDIR)/$(LIBTERMKEYLIB) -o $@ $(LDFLAGS)

libtermkey:
	make -C $(LIBTERMKEYDIR)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(BINARY)
	make -C $(LIBTERMKEYDIR) clean

