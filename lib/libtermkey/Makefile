CC=cc
AR=ar
CFLAGS=-c -std=c99 -O2
SOURCES=termkey.c driver-ti.c driver-csi.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=libtermkey.a

.PHONY: all
all: $(SOURCES) $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $(LIBRARY) $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf *.o $(LIBRARY)

