/*
 * memlib.c - a module that simulates the memory system. Adapted from CSAPP3e.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "mem.h"

#ifndef MAX_HEAP
#define MAX_HEAP (4 * (1 << 20))  // 4 MB
#endif

static char *mem_heap;     /* Points to first byte of heap */
static char *mem_brk;      /* Points to last byte of heap plus 1 */
static char *mem_max_addr; /* Max legal heap addr plus 1*/

/*
 * mem_init - Initialize the memory system model.
 *            Allocate an arena of size MAX_HEAP.
 */
void mem_init(void)
{
    mem_heap = mmap(NULL, MAX_HEAP, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem_heap == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    mem_brk = (char *) mem_heap;
    mem_max_addr = (char *)(mem_heap + MAX_HEAP);
}

/*
 * mem_sbrk - Simple model of the sbrk function. Extends the heap
 *            by incr bytes and returns the start address of the new area.
 *            In this model, the heap cannot be shrunk.
 */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;

    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
        errno = ENOMEM;
        return (void *) -1;
    }
    mem_brk += incr;
    return (void *) old_brk;
}

/*
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    munmap(mem_heap, MAX_HEAP);
}
