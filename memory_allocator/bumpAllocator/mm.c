#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#include "mm.h"
#include "mem.h"

#define DWORD_SIZE   16    // Defines the alignment boundary for memory allocation (16-byte alignment).
#define PAGE_SIZE  4096    // Defines the size of a page (4096 bytes), used to grow the heap.

static void *heap_ptr = NULL;        // Pointer to the start of the heap.
static size_t remaining_heap = 0;    // Tracks the amount of remaining space in the heap.

void mm_init(void)
{
    mem_init();  // Initializes the memory system.

    // Grow the heap by one page (PAGE_SIZE) during initialization.
    if ((heap_ptr = mem_sbrk(PAGE_SIZE)) == (void *) -1) {
        perror("mem_sbrk");  // Handle the failure of mem_sbrk.
        exit(1);
    }
    
    // Ensure the heap is properly aligned to a 16-byte boundary.
    assert(((uint64_t) heap_ptr) % DWORD_SIZE == 0);
    remaining_heap = 4096;  // Initialize the remaining heap size to the size of a single page (4096 bytes).
}

void *mm_malloc(size_t size)
{
    assert(((uint64_t) heap_ptr) % DWORD_SIZE == 0);  // Ensure heap pointer is 16-byte aligned.

    size_t asize;      /* Adjusted block size, taking overhead and alignment into account. */
    size_t extendsize; /* Amount to extend the heap if there's not enough memory left. */

    // Initialize the heap if it hasn't been initialized yet.
    if (heap_ptr == NULL) {
        mm_init();
    }

    // Return NULL if a zero-size request is made (not valid in this implementation).
    if (size == 0) {
        return NULL;
    }

    // Brute force approach to memory allocation: adjust the requested size to ensure alignment.
    /* Adjust block size to include overhead (header/footer) and alignment requirements. */
    if (size <= DWORD_SIZE)
        asize = 2 * DWORD_SIZE;  // Ensure at least a block of 16 bytes.
    else
        asize = DWORD_SIZE * ((size + (DWORD_SIZE) + (DWORD_SIZE - 1)) / DWORD_SIZE); // Round up to next multiple of DWORD_SIZE.

    // If the requested block size exceeds the remaining heap size, we need to extend the heap.
    if (asize > remaining_heap) {
        size_t shortfall = asize - remaining_heap;  // Calculate how much more memory we need.

        // If the shortfall is greater than the available page size, we extend by double the page size.
        extendsize = (shortfall <= PAGE_SIZE) ? PAGE_SIZE : 2 * PAGE_SIZE;

        // Request additional memory from the operating system to satisfy the shortfall.
        if ((heap_ptr = mem_sbrk(extendsize)) == (void *) -1) {
            perror("mem_sbrk");  // Handle failure of mem_sbrk.
            exit(1);
        }

        // Update the remaining heap size after extending the heap.
        remaining_heap += extendsize;
    }

    // Allocate the block from the current heap position.
    void *allocated_block = heap_ptr;
    heap_ptr += asize;  // Move the heap pointer forward by the size of the allocated block.
    remaining_heap -= asize;  // Update the remaining heap size.

    /* Ensure alignment of the returned address (debug check). */
    assert(((uintptr_t)allocated_block) % DWORD_SIZE == 0);  // Verify that the allocated block is 16-byte aligned.

    // For debugging purposes, print the allocation details.
    printf("Allocated block at: %p, size: %zu bytes, remaining_heap: %zu bytes\n", allocated_block, asize, remaining_heap);

    return allocated_block;  // Return the pointer to the allocated block.
}

void mm_free(void *p)
{
    // No-op: This function does not do anything in this simple implementation.
    // Memory freeing would require a more complex implementation such as a free list or coalescing blocks.
}

void mm_deinit(void)
{
    mem_deinit();  // De-initialize the memory system when done.
}
