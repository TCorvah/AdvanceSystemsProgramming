CC=gcc
CFLAGS=-g -Wall
.PHONY: default
default: apu thread 
apu: error.o a.out
thread: error.o
.PHONY: clean
clean:
	rm -f apu thread *.o
	rm -rf *.dYSM
.PHONY: all
all: clean default