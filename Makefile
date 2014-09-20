CC=gcc
CFLAGS=-c -std=c99 -Wall -Wextra -Werror -pedantic -g
#CFLAGS=-c -std=c99 -O2 -Wall -Wextra -pedantic
LDFLAGS=-lncursesw
SOURCES=wed.c display.c buffer.c util.c edit.c session.c status.c command.c file.c variable.c
OBJECTS=$(SOURCES:.c=.o)
BINARY=wed

all: $(SOURCES) $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(BINARY)

