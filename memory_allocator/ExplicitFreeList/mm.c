#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <errno.h>


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
#define ALIGN(size) (((size) + 0xF) & ~0xF)

/*i
 * Block Header and Footer Structures:
 * - `size`: Block size in bytes (60 bits).
 * - `allocated`: Allocation status (1 bit: 0 = free, 1 = allocated).
 */
typedef struct header {
    uint64_t      size : 60;
    uint64_t    unused :  3;
    uint64_t allocated :  1;

    union {
        // These links overlap with the first 16 bytes of a block's payload.
        struct {
            struct header *fprev, *fnext;
        } links;

        char payload[0];
    };
}header_t; 

//static header_t *sentinel = NULL;

typedef struct {
    uint64_t      size : 60;
    uint64_t    unused :  3;
    uint64_t allocated :  1;
}footer_t;

typedef struct aligned_header{
    header_t header;
    char padding[8 - sizeof(header_t *) % 8];
}aligned_header;


static inline int MAX(int x, int y) {
    return (x > y) ? x : y;
}

/* Helper function prototypes */
static inline header_t *header(void *payload);
static inline footer_t *footer(void *payload);
static inline void *next_payload(void *payload);
static inline void *prev_payload(void *payload);
static inline void *next_free_payload(void *payload);
static inline void *prev_free_payload(void *payload);


/*
 * Header: Returns a pointer to the header of the block containing current `payload`.
 */
static inline header_t *header(void *payload) {
    // Implement this function
    //assert((uint64_t)payload % DWORD_SIZE == 0);
    header_t * hdr_addr = (header_t *)((char *)payload - WSIZE);
    return hdr_addr;
     
}

/*
 * Footer: Returns a pointer to the footer of the block containing  current `payload`.
 */
static inline footer_t *footer(void *payload) {
    footer_t *ftr_addr = (footer_t *)((char *)payload + header(payload)->size 
                            - WSIZE - sizeof(footer_t));
   return ftr_addr;
   
}

static inline void freeList(void* node) {
    header_t *node_header = header(node);  // Get the header of the node to be freed
    header_t *sentinel_node = node_header;    // Get the sentinel node (head of free list)

    // Check if the free list is empty
    if (sentinel_node->links.fnext == sentinel_node) {
        // The free list is empty, set both fnext and fprev to point to the sentinel
        sentinel_node->links.fnext = sentinel_node->links.fprev = node_header;
        node_header->links.fnext = sentinel_node;
        node_header->links.fprev = sentinel_node;
    } else {
        // The free list is not empty, add the node at the end of the list (LIFO)
        header_t *last_node = sentinel_node->links.fprev;  // Last node in the free list
        
        // Update the links to add the new node at the end (LIFO)
        last_node->links.fnext = node_header;
        node_header->links.fprev = last_node;
        node_header->links.fnext = sentinel_node;
        sentinel_node->links.fprev = node_header;
    }
}

/*
 * Next Payload: Returns a pointer to the payload of the next block.
 */
static inline void *next_payload(void *payload) {
    char *p = (char*)payload + ALIGN(header(payload)->size);
    return p; 
}

/* The following function takes you to the previous payload in the heap */
static inline void *prev_payload(void *payload) {
    footer_t *prev_ftr = (footer_t *)((char *)header(payload) - sizeof(footer_t));
    size_t block_size = prev_ftr->size;
    char *p = (char *)payload - block_size;
    return p;
}
static inline void *next_free_payload(void *payload) {
    // Implement this function
    return  header(payload)->links.fnext->payload;
}

static inline void *prev_free_payload(void *payload) {
    // Implement this function
    return header(payload)->links.fprev->payload;
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
    // Block payload should be WORD_SIZE bytes after the beginning of a header
    assert(sizeof(header_t) == 3 * WSIZE);
    // Block payload should be WORD_SIZE bytes after the beginning of a header
    assert(offsetof(header_t, payload) == WSIZE);
    assert(sizeof(footer_t) == WSIZE);


    mem_init();
    char *p;
    /* Create the initial empty heap */
    if ((p = mem_sbrk( 6 * WSIZE) ) == (void *)-1) {
        perror("mem_sbrk");
        exit(1);
    }
      // 8-byte leading padding
    memset(p, 0, WSIZE);
    p += DWORD_SIZE;

    // prologue block is the sentinel and needs to be modify to account for minimum payload of 16 byte
   // heap_listp = heap_listp + (2 * WSIZE);
    header(p)->size = 2 * DWORD_SIZE;
    header(p)->allocated = 1;
    footer(p)->size = 2 * DWORD_SIZE;
    footer(p)->allocated = 1;

    // Initialize empty circular seglist
    header(p)->links.fprev = header(p)->links.fnext = header(p);
 
 
    // Initialize epilogue
    p +=  2 * DWORD_SIZE;
    header((p))->size = 0;
    header((p))->allocated = 1;
  

    heap_listp = (char *)p -  (2 * DWORD_SIZE);
 
    // Extend the empty heap with a free block of PAGE_SIZE bytes
    if (extend_heap(CHUNKSIZE/DWORD_SIZE) == NULL) {
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

    if (heap_listp == NULL){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size <=  0){
        return NULL;
    }
     if (size >  (CHUNKSIZE * 4)){
        errno = ENOMEM;
     }
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DWORD_SIZE){
        asize = 2*DWORD_SIZE;
    }else{
        asize = DWORD_SIZE * ((size + (DWORD_SIZE) + (DWORD_SIZE-1)) / DWORD_SIZE);
    }
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    //extendsize = MAX(asize,CHUNKSIZE);
    //extendsize = ((asize + CHUNKSIZE - 1) / CHUNKSIZE) * CHUNKSIZE;
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Free a block
 */
void mm_free(void *bp)
{
    //size_t block_size = 0;
    
    // merge adjacent blocks into larger blocks to prevent external fragmentation
    //coalesce(bp);
    if (bp == NULL) {
        return;
    }
    
    //shift left
    //block_size = header(bp)->size << 4;
    header(bp)->allocated = footer(bp)->allocated = 0;
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 this deals with the issue of merging free blocks
 */
static void *coalesce(void *bp)
{
    void *prev = prev_payload(bp);
    void *next = next_payload(bp);

    uint64_t prev_alloc = header(prev)->allocated;
    uint64_t next_alloc = header(next)->allocated;

   uint64_t current_payload_size = header(bp)->size;

   // Global sentinel (free list head)

    

    if (prev_alloc && next_alloc) {
        /* Case 1: No coalescing needed, both previous and next blocks are allocated */
        //return bp;
    } else if (prev_alloc && !next_alloc) {
        /* Case 2: Coalesce with the next block, previous block is allocated */
        current_payload_size += header(next)->size;
        header(bp)->size = current_payload_size;
        footer(bp)->size = current_payload_size;

        // Remove next from the free list
        //freeList(next);  // Add function to remove 'next' block from free list
          // Remove next from free list
        header(next)->links.fprev->links.fnext = header(next)->links.fnext;
        header(next)->links.fnext->links.fprev = header(next)->links.fprev;

    } else if (!prev_alloc && next_alloc) {
        /* Case 3: Coalesce with the previous block, next block is allocated */
        current_payload_size += header(prev)->size;
        footer(bp)->size = current_payload_size;
        header(prev)->size = current_payload_size;

        // Remove prev from the free list
        //freeList(prev);  // Add function to remove 'prev' block from free list
           // Remove prev from free list
        header(prev)->links.fprev->links.fnext = header(prev)->links.fnext;
        header(prev)->links.fnext->links.fprev = header(prev)->links.fprev;

        // Coalesced free block starts at prev now
        bp = prev;
    } else {
        /* Case 4: Coalesce with both previous and next blocks */
        current_payload_size += header(prev)->size + footer(next)->size;
        header(prev)->size = current_payload_size;
        footer(next)->size = current_payload_size;

        // Remove both prev and next from the free list
        //freeList(prev);  // Remove 'prev' block from free list
        //freeList(next);  // Remove 'next' block from free list
            // Remove prev and next from free list
        header(prev)->links.fprev->links.fnext = header(prev)->links.fnext;
        header(prev)->links.fnext->links.fprev = header(prev)->links.fprev;
        header(next)->links.fprev->links.fnext = header(next)->links.fnext;
        header(next)->links.fnext->links.fprev = header(next)->links.fprev;

        // Coalesced free block starts at prev now
        bp = prev;
    }

    // Add coalesced block to the free list
    //freeList(bp);  // After coalescing, add the resulting block back to the free list
       // Add coalesced block to beginning of free list
    header_t *sentinel_node = header(bp);
    header(bp)->links.fprev = sentinel_node;
    header(bp)->links.fnext = sentinel_node->links.fnext;
    sentinel_node->links.fnext->links.fprev = header(bp);
    sentinel_node->links.fnext = header(bp);

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
    size = (words % 2) ? (words+1) * DWORD_SIZE : words * DWORD_SIZE;

    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    header(bp)->size = size;        /* Free block header */
    footer(bp)->size = size;
    header(bp)->allocated = footer(bp)->allocated = 0;

    header(bp)->links.fnext = header(bp)->links.fprev = header(bp);
   
     /* New epilogue header */
    header(next_payload(bp))->size = 0;
    header(next_payload(bp))->allocated = 1; 
   

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *p, size_t asize)
{
    uint64_t current_size = header(p)->size;

    // Get previous free block and next free payloads
    void *fprev_p = prev_free_payload(p);
    void *fnext_p = next_free_payload(p);

    if ((current_size - asize) >= (2 * DWORD_SIZE)) {

        header(p)->size = asize;
        footer(p)->size = asize;
        header(p)->allocated = footer(p)->allocated = 1;

        // q is a new free block left over after placing p
        void *q = next_payload(p);
        header(q)->size = current_size - asize;
        footer(q)->size = current_size - asize;
        header(q)->allocated = footer(q)->allocated = 0;

        // Add q to the free list in p's place
        header(fnext_p)->links.fprev = header(q);
        header(fprev_p)->links.fnext = header(q);
        header(q)->links.fprev = header(fprev_p);
        header(q)->links.fnext = header(fnext_p);
    } else {
        // There was no leftover, simply remove p from the free list
        header(fnext_p)->links.fprev = header(fprev_p);
        header(fprev_p)->links.fnext = header(fnext_p);

        header(p)->allocated = footer(p)->allocated = 1;
    }
}
/*
 * find_fit - Find a fit for a block with asize bytes
 */


static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *p;
    for (p = next_free_payload(heap_listp); p != heap_listp;
         p = next_free_payload(p)) {
        if (asize <= header(p)->size) {
            return p;
        }
    }
    return NULL;
}

static void printblock(void *p)
{
    uint64_t hsize, halloc, fsize, falloc, plinks;

    hsize = header(p)->size;
    halloc = header(p)->allocated;
    fsize = footer(p)->size;
    falloc = footer(p)->allocated;

    // Print free list links for free blocks and the sentinel node
    plinks = !halloc || p == heap_listp;

    if (hsize == 0) {
        printf("%p: EOL\n", p);
        return;
    }

    if (plinks) {
        printf("%p: header: [%llu:%c] {%p|%p} footer: [%llu:%c]\n", p,
            hsize, (halloc ? 'a' : 'f'),
               header(p)->links.fprev->payload,
               header(p)->links.fnext->payload,
               fsize, (falloc ? 'a' : 'f'));

    } else {
        printf("%p: header: [%llu:%c] {} footer: [%llu:%c]\n", p,
               hsize, (halloc ? 'a' : 'f'),
               fsize, (falloc ? 'a' : 'f'));
    }
}

static void checkblock(void *bp)
{
   if ((char *)bp < heap_listp) {
    printf("Error: Out-of-bounds access at %p\n", bp);
    return;
}
    if ((unsigned long)bp % DWORD_SIZE)
        printf("Error: %p is not doubleword aligned\n", bp);

    if (header(bp)->size != footer(bp)->size)
        printf("Error: header does not match footer\n");
        

    if (header(bp)->allocated == 1 && footer(bp)->allocated == 0)
        printf("Error: header cannot be allocated and footer free or vice versa\n");
}


/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheaps(int verbose)
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
/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    void *p = heap_listp;

    if (verbose) {
        printf("Heap (%p):\n", heap_listp);
    }

    // Check prologue
    if (header(heap_listp)->size != 2 * DWORD_SIZE ||
        header(heap_listp)->allocated == 0) {
        printf("Bad prologue header\n");
        exit(1);
    }
    checkblock(heap_listp);

    // Check all blocks
    for (p = heap_listp; header(p)->size > 0; p = next_payload(p)) {
        if (verbose) {
            printblock(p);
        }
        checkblock(p);
    }

    // Check epilogue
    if (verbose) {
        printblock(p);
    }
    if (header(p)->size != 0 || header(p)->allocated == 0) {
        printf("Bad epilogue header\n");
        exit(1);
    }

    // Check free list consistency
    for (p = next_free_payload(heap_listp); p != heap_listp;
         p = next_free_payload(p)) {
        if (header(p)->allocated) {
            printf("Free block marked as allocated: %p\n", p);
            exit(1);
        }

        if (header(p)->links.fnext->links.fprev != header(p) ||
            header(p)->links.fprev->links.fnext != header(p)) {
            printf("Free list pointers inconsistent: %p\n", p);
            exit(1);
        }
    }
}
