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
 * Explicit Free List Memory Allocator
 * 
 * This implementation manages the heap  memory by creating 
 * our own explicit allocator for free and malloc that manages blocks of 
 * virtual memory from the heap.
 *  our helper function that helps creates malloc are the following
 * 
 * - First-fit placement strategy
 * - Boundary tag coalescing for adjacent free blocks
 * - next and previous pointers for free payloads
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

/*
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
static inline header_t *remove_from_freelist(void *payload);
static inline void *add_merge_block_to_freelist(void *bp);
static inline void *next_payload(void *payload);
static inline void *prev_payload(void *payload);
static inline void *next_free_payload(void *payload);
static inline void *prev_free_payload(void *payload);


/*
 * Header: Returns a pointer to the header of the block containing current `payload`.
 */
static inline header_t *header(void *payload) {
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
static inline header_t *remove_from_freelist(void *payload){
    if (!payload) {
        return NULL; // Return NULL if payload is invalid
    }
    
    header_t *head = header(payload);

    if (head->links.fprev) {
        head->links.fprev->links.fnext = head->links.fnext;
    }
    if (head->links.fnext) {
        head->links.fnext->links.fprev = head->links.fprev;
    }

    return head;
}

static inline void *add_merge_block_to_freelist(void *bp){
     if (!bp) {
        return NULL; // Return NULL if payload is invalid
    }
    // save a pointer of the current header block
    header_t *merge_block = header(bp);

    header(bp)->links.fprev = merge_block; // a pointer of the merge block is save in the tail of the list
    header(bp)->links.fnext = merge_block->links.fnext; 
    merge_block->links.fnext->links.fprev = header(bp); // header is save in the tail of the merge block
    merge_block->links.fnext = header(bp); // save header in the next of the merge block
  
    return bp;
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
    
    return  header(payload)->links.fnext->payload;
}

static inline void *prev_free_payload(void *payload) {
    // Implement this function
    return header(payload)->links.fprev->payload;
}



/* Global pointer to the start of the heap, (char *) 
is store as 8 byte quad word on a x86-64  machine*/
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
 * mm_init - Initialize the memory manager for our explicit allocator
 for program correctness, since this is a 64 bit implementatation, our expected
 return address should be a multiple of 16 bytes
 */
void mm_init(void)
{
    // the struct for header_t is 24 bytes in total, 8 bytes for the header,
    // 8 bytes each for the previous and next pointer in the struct.
    assert(sizeof(header_t) == 3 * WSIZE);
    //  should be WORD_SIZE bytes after the beginning of a header
    assert(offsetof(header_t, payload) == WSIZE);
    // assert footer is 8 bytes
    assert(sizeof(footer_t) == WSIZE);


    mem_init();
    //char *p; // 8 bytes pointer 
    /* Create the initial empty heap , the heap is intialize with 48 bytes in total
     32 bytes for the header, next, previous and footer(prologue block)
     8 byte for the epilogue block and 8 byte for the initial heap pointer that starts 
     after the prologue blo but before the epilogue header*/
    if ((heap_listp = mem_sbrk( 6 * WSIZE) ) == (void *)-1) {
        perror("mem_sbrk");
        exit(1);
    }
    // 8-byte leading padding
    memset(heap_listp, 0, WSIZE);

    heap_listp += DWORD_SIZE;

    // prologue block is the sentinel and needs to be modify to account for minimum payload of 16 byte
    header_t *prologue_hdr = header(heap_listp);
    prologue_hdr->size = 2 * DWORD_SIZE;
    prologue_hdr->allocated = 1;
    prologue_hdr->links.fprev = prologue_hdr->links.fnext = prologue_hdr;
    footer_t *prologue_ftr = footer(heap_listp);
    prologue_ftr->size = 2 * DWORD_SIZE;
    prologue_ftr->allocated = 1;

 
    // Initialize epilogue
    heap_listp +=  2 * DWORD_SIZE;
    header_t *epilogue_hdr = header(heap_listp);
    epilogue_hdr->size = 0;
    epilogue_hdr->allocated = 1;
  

    heap_listp = (char *)heap_listp -  (2 * DWORD_SIZE);
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
 * mm_malloc - Allocate a block with at least size bytes of payload.
 the minimum block size that is 32 bytes or 4 words, 16 bytes payload, 8 bytes header,
  8 byte footer. The data returned by malloc must be
 16 byte aligned including the block address in conformity with a 64 bit machine. 
 Since the heap grows upwards, for correctness, the address size must be increasing and not decreasing
 if a user request 8 bytes, we round it up to 16 and add 8 bytes for header and 8 bytes for footer
 if a user request 20 bytes we allocate 48 bytes, 
 we round up to the nearest power of two which is 32 bytes plus header/footer
 In this explicit free list implementation, we only traverse the free list to find a block in memory
 that has been recently free, as newly free blocks are inserted at teh head of the list
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size for alignment*/
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == NULL){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size <=  0){
        return NULL;
    }
     if (size >  (CHUNKSIZE * 4)){ // I set a constraint to the max chunksize that a user can request
        errno = ENOMEM;
     }
    /* Adjust block size to include overhead and alignment reqs., note that this condition
    can cause internal fragmentation if for instance a user keep requesting a smaller payload
    and we keep padding for alignmenet, the unused space is what is known as the fragmentated space
    on the heap*/
    if (size <= DWORD_SIZE){ // this case is when a user request less than 16 bytes, this case
        asize = 2*DWORD_SIZE; // usually doesn't ;ead to external fragmentation
    }else{
        asize = DWORD_SIZE * ((size + (DWORD_SIZE) + (DWORD_SIZE-1)) / DWORD_SIZE); //when a user request more than 16 bytes but less than the allowable size
    }
    /* Search the free list for a fit , not that there may be lots of small free blocks that could
    sum up a user request if they are laid out in one contiguos block in memory. 
    An external fragmentation occurs when a user request a block size and we cannot find a fit
    even though we have enough empty block on the heap, the coalescong helps to fix this issue*/
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found even with the coalesce, we extend the heap by a chunksize of memory and place the block
    at the front of the free list */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/DWORD_SIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Free a block, we will use a free list to keep track of the free blocks in the heap
 */
void mm_free(void *bp)
{
    
    // check if the block pointer is null
    if (bp == NULL) {
        return;
    }
    //header_t *new_free_hdr = header(bp);
    //size_t size = header(bp)->size << 4;
    if (heap_listp == NULL){
        mm_init();
    }
    
    //footer_t *prev_blk_ftr = footer(prev_payload(bp));
    //footer_t *curr_blk_ftr = footer(bp);
    //header_t *new_blk = header(next_payload(bp));
    //if(prev_blk_ftr->allocated == 1 && )
    //header(bp)->size = footer(bp)->size = size;
    header(bp)->allocated = footer(bp)->allocated = 0;
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 this deals with the issue of merging free blocks to help reduce external fragmentation
 and increases the likelihood of finding contiguios blocks in memory.
 all newly merge free blocks will be place at the front of the free list
  for easy insertion and removal
 */
static void *coalesce(void *bp)
{
    void *prev = prev_payload(bp);
    void *next = next_payload(bp);

    size_t prev_alloc = header(prev)->allocated;
    size_t next_alloc = header(next)->allocated;

    size_t current_payload_size = header(bp)->size; 

    if (prev_alloc == 1 && next_alloc == 1) {
        /* Case 1: No coalescing needed, both previous and next blocks are allocated */
        return bp;
    } else if (prev_alloc == 1 && next_alloc == 0) {
        /* Case 2: Coalesce with the next block, previous block is allocated */
        current_payload_size += header(next)->size;
        header(bp)->size = current_payload_size;
        footer(bp)->size = current_payload_size;

        // Remove next from the free list
        remove_from_freelist(next);

    } else if (prev_alloc == 0 && next_alloc == 1) {
        /* Case 3: Coalesce with the previous block, next block is allocated */
        current_payload_size += header(prev)->size;
        footer(bp)->size = current_payload_size;
        header(prev)->size = current_payload_size;

        // Remove prev from the free list
        remove_from_freelist(prev);

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
        remove_from_freelist(prev);
        remove_from_freelist(next);
        // Coalesced free block starts at prev now
        bp = prev;
    }
    // Add coalesced block to beginning of free list
    add_merge_block_to_freelist(bp);
    
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

    /* Allocate an even number of words to maintain alignment, 
    not too sure if I should divide by double word or word*/
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
 *  this technique can sometimes introduce internal fragmentation.
 *  
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
 * find_fit - Find a fit for a block with asize bytes, this function searches the free list
 by looking at the next and previous free payload
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
