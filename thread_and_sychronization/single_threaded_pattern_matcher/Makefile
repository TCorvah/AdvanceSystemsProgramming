CC=gcc
CFLAGS=-g -Wall

# Default to using heap-allocated buffers. 
# To use mmap, run `make USE_MMAP=1`
USE_MMAP=0

ifneq ($(USE_MMAP),0)
	CFLAGS += -DUSE_MMAP
endif

greptile:

.PHONY: clean
clean:
	rm -f -- ./*.o greptile

.PHONY: all
all: clean greptile
