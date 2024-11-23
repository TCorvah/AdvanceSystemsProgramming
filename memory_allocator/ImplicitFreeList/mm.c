#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include<inttypes.h>
#include <stddef.h>

#include "mm.h"
#include "../libmem/mem.h"


/*
 * Implicit Free List Memory Allocator
 * 
 * This implementation manages memory using an implicit free list with:
 * - First-fit placement strategy
 * - Boundary tag coalescing for adjacent free blocks
 * - 16-byte alignment for payloads
 * - Minimum block size of 32 bytes (header, footer, and alignment padding)
 *
 * Key Features:
 * - Header and footer include size (60 bits) and allocation status (1 bit).
 * - Blocks are coalesced when freed to reduce fragmentation.
 * - Memory is extended as needed using `mem_sbrk`.
 */


#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DWORD_SIZE  16      /* Double-word size (bytes) */
#define CHUNKSIZE   4096    /* Extend heap by this amount (bytes) */

/*
 * Block Header and Footer Structures:
 * - `size`: Block size in bytes (60 bits).
 * - `allocated`: Allocation status (1 bit: 0 = free, 1 = allocated).
 */
typedef struct {
   // Note that this diverges from CSAPP's header semantics. Here, the size
   // field stores the full block size in bytes using 60 bits. This is more
   // than sufficient -- Linux uses 48 or 57 bits for virtual addresses. We do
   // not use any part of those 60 bits to store the allocated bit. The three
   // bits after size are reserved for future use and the last bit is the
   // allocated bit.
    uint64_t      size : 60;
    uint64_t    unused :  3;
    uint64_t allocated :  1;

    char payload[];
} header_t;

typedef struct {
    uint64_t      size : 60;
    uint64_t    unused :  3;
    uint64_t allocated :  1;
} footer_t;


static inline int MAX(int x, int y) {
    return (x > y) ? x : y;
}
/* Helper function prototypes */
static inline header_t *header(void *payload);
static inline footer_t *footer(void *payload);
static inline void *next_payload(void *payload);
static inline void *prev_payload(void *payload);
/*
 * Header: Returns a pointer to the header of the block containing current `payload`.
 */
static inline header_t *header(void *payload) {
    // Implement this function
     header_t * hdr_addr = (header_t *)((char *)payload - WSIZE);
     return hdr_addr;
     
}

/*
 * Footer: Returns a pointer to the footer of the block containing  current `payload`.
 */
static inline footer_t *footer(void *payload) {
    footer_t *ftr_addr = (footer_t *)((char *)payload + header(payload)->size - DWORD_SIZE);
    return ftr_addr;
   
}
/*
 * Next Payload: Returns a pointer to the payload of the next block.
 */
static inline void *next_payload(void *payload) {
    char *p = (char*)payload + header(payload)->size;
    return p; 
}

/* The following function takes you to the previous payload in the heap */
static inline void *prev_payload(void *payload) {
    footer_t *prev_ftr = (footer_t *)((char *)header(payload) - DWORD_SIZE);
    size_t block_size = prev_ftr->size;
    char *p = (char *)payload - block_size;
    return p;
}

/* Global pointer to the start of the heap */
static char *heap_listp = 0;  

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkheap(int verbose);
static void checkblock(void *bp);

/*
 * mm_init - Initialize the memory manager
 */
void mm_init(void)
{

    assert(sizeof(header_t) == WSIZE);

    assert(sizeof(footer_t) == WSIZE);
    
  
    
    mem_init();

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        perror("mem_sbrk");
        exit(1);
    }
      // 8-byte leading padding
    memset(heap_listp, 0, WSIZE);

    printf("heap list current address: %p bytes\n", heap_listp);

    // a bug is detected in the mm_init logic for the prologue and epilogue block address
    heap_listp += DWORD_SIZE;
    header(heap_listp)->size = DWORD_SIZE;
    header(heap_listp)->allocated = 1;
    printf("Prologue header address: %p\n", header(heap_listp));
    
    // My understanding is that the pointer should be incremented by a word size
    //for the prologue header and a word size for the prologue footer
    // so 16 byte total
    heap_listp += DWORD_SIZE;
    footer(heap_listp)->size = DWORD_SIZE;
    footer(heap_listp)->allocated = 1;
    printf("Prologue footer address: %p\n", footer(heap_listp));

    // Initialize epilogue
    //epilogue header sometimes has alignment issues that could be from the coalesce function
    // or the extend heap.
    //the epilogue should be at the end of the heap,
    header(heap_listp);
    header(heap_listp)->size = 0;
    header(heap_listp)->allocated = 1;
    printf("epilogue header address: %p\n", heap_listp);
    heap_listp -= DWORD_SIZE;


    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        perror("extend_heap");
        exit(1);
    }
}

void mm_deinit(void)
{
    mem_deinit();
}

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size <=  0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DWORD_SIZE)
        asize = 2*DWORD_SIZE;
    else
        asize = DWORD_SIZE * ((size + (DWORD_SIZE) + (DWORD_SIZE-1)) / DWORD_SIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Free a block
 */
void mm_free(void *bp)
{

    if (bp == 0)
        return;

    size_t block_size = header(bp)->size;
    if (heap_listp == 0){
        mm_init();
    }
   
    // free the header block with size intact, block is mark as free in header
    header(bp)->size = block_size;
    header(bp)->allocated = 0;

    // free the footer block with size intact
    footer(bp)->size = block_size;
    footer(bp)->allocated = 0;
    
    // merge adjacent blocks into larger blocks to prevent external fragmentation
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = footer(prev_payload(bp))->allocated;
    size_t next_alloc = header(next_payload(bp))->allocated;
    size_t block_size = header(bp)->size;
    
    if (prev_alloc == 1 && next_alloc == 1) {            /* Case 1, no coalescing needed, blocks are not free */
        return bp;
    }
    else if (prev_alloc == 1 && next_alloc == 0) {      /* Case 2, coalesce, prev is not free but the next is free */
        block_size += header(next_payload(bp))->size;
        header(bp)->size = block_size;
        header(bp)->allocated = 0;

        footer(bp)->size = block_size;
        footer(bp)->allocated = 0;
       
    }
    else if (prev_alloc == 0 && next_alloc == 1) {      /* Case 3 , coalesce, prev is free but next is allocated*/
        block_size +=  header(prev_payload(bp))->size;
        footer(bp)->size = block_size;
        footer(bp)->allocated = 0;

        header(prev_payload(bp))->size = block_size; 
        header(prev_payload(bp))->allocated = 0;  
        bp = prev_payload(bp);

    }
    else {                                     /* Case 4 */
        block_size += header(prev_payload(bp))->size + 
        footer(next_payload(bp))->size;
        header(prev_payload(bp))->size = block_size;
        header(prev_payload(bp))->allocated = 0; 

        footer(next_payload(bp))->size = block_size;
        footer(next_payload(bp))->allocated = 0;
        bp = prev_payload(bp);
    }

    return bp;
}

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = header(ptr)->size;
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/*
 * mm_checkheap - Check the heap for correctness
 */
void mm_checkheap(int verbose)
{
    checkheap(verbose);
}

/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1) 
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    header(bp)->size = size;
    header(bp)->allocated = 0;         /* Free block header */
    footer(bp)->size = size;
    footer(bp)->allocated = 0;
       /* Free block footer */
    header(next_payload(bp))->size = 0;
    header(next_payload(bp))->allocated = 1; 
    /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = header(bp)->size;

    if ((csize - asize) >= (2*DWORD_SIZE)) {
        header(bp)->size = asize;
        header(bp)->allocated = 1; 
        footer(bp)->size = asize;
        footer(bp)->allocated = 1; 
       
        bp = next_payload(bp);
        header(bp)->size = csize - asize;
        header(bp)->allocated = 0;  

        footer(bp)->size = csize - asize;
        footer(bp)->allocated = 0;  
    }
    else {
        header(bp)->size = csize;
        header(bp)->allocated = 1; 

        footer(bp)->size = csize;
        footer(bp)->allocated = 1;  
    }
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *bp;

    for (bp = heap_listp; header(bp)->size > 0; bp = next_payload(bp)) {
        if (header(bp)->allocated == 0 && asize <= header(bp)->size) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = header(bp)->size;
    halloc = header(bp)->allocated;
    fsize = footer(bp)->size;
    falloc = footer(bp)->allocated;

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
    if ((uintptr_t)bp % DWORD_SIZE)
        printf("Error: %p is not doubleword aligned\n", bp);

    if (header(bp)->size != footer(bp)->size)
        printf("Error: header does not match footer\n");

    if (header(bp)->allocated == 1 && footer(bp)->allocated == 0)
        printf("Error: header cannot be allocated and footer free or vice versa\n");
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;
  
    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if (header(heap_listp)->size != DWORD_SIZE || 
    header(heap_listp)->allocated == 0) {
        printf("Bad prologue header\n");
     checkblock(heap_listp);
    }
     if (footer(heap_listp)->size != DWORD_SIZE || 
    footer(heap_listp)->allocated == 0) {
        printf("Bad prologue header\n");
     checkblock(heap_listp);
    }
    

    for (bp = heap_listp; header(bp)->size > 0; bp = next_payload(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    //epilogue header
    if (header(bp)->size != 0 || header(bp)->allocated == 0)
        printf("Bad epilogue header\n");
}
