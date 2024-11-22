CC=gcc
CFLAGS=-g -Wall -I../libmem    # Include path to mem.h (if it's in ../libmem)
LDFLAGS=-L../libmem            # Path to libmem.a (if it's in ../libmem)
LDLIBS=-lmem                   # Link against libmem.a

# Targets
mm-test: mm.o mm-test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mm-test mm.o mm-test.o $(LDLIBS)

mm.o: mm.h
	$(CC) $(CFLAGS) -c mm.c

mm-test.o: mm.h
	$(CC) $(CFLAGS) -c mm-test.c

# Clean up build artifacts
.PHONY: clean
clean:
	rm -f *.o mm-test

# Default target
.PHONY: all
all: clean mm-test