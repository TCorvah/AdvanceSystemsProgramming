CC=gcc
CFLAGS=-g -Wall -I../libmem
LDFLAGS=-L../libmem
LDLIBS=-lmem

mm-test: mm.o mm-test.o

mm.o: mm.h

mm-test.o: mm.h

.PHONY: clean
clean:
	rm -f *.o mm-test

.PHONY: all
all: clean mm-test
