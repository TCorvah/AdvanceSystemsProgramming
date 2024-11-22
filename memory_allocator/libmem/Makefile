CC=gcc
CFLAGS=-g -Wall

libmem.a: mem.o
	ar rcs libmem.a mem.o

mem.o: mem.h

.PHONY: clean
clean:
	rm -f *.o *.a

.PHONY: all
all: clean libmem.a
