/*
 * This code is based on a 32-bit memory allocator implementation from the 
 * CS:APP3e textbook, which has been updated and adapted for a 64-bit system. 
 * It utilizes implicit free lists, first-fit placement, and boundary tag 
 * coalescing as described in the textbook. All memory blocks are aligned to 
 * doubleword (16-byte) boundaries, and the minimum block size is 32 bytes. 
 * The prologue block is an exception, as it does not include a payload in 
 * this implementation.
 * 
 * Inline functions have been used in place of macros to improve code 
 * readability and simplify debugging. Unlike macros, which can obscure 
 * errors during compile time, inline functions provide better error 
 * reporting and are easier to debug at runtime.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h> // For size_t

#include "mm.h"
#include "../libmem/mem.h"

/* Basic constants and macros */
#define WSIZE   8   /* Word and header/footer size (bytes) */
#define DSIZE       16       /* Double word size (bytes) */
#define CHUNKSIZE  4096 /* Extend heap by this amount (bytes) */
#define MIN_BLOCK_SIZE 32


/* Helper function prototypes */
static inline void *hdr_pointer(void *payload);
static inline void *ftr_pointer(void *payload);
static inline void *next_blkp(void *payload);
static inline void *prev_blkp(void *payload);


/* Pack a size and allocated bit into a word */
static inline size_t pack(size_t size, size_t alloc) {
    return size | alloc;  // Combine the size and allocation bit
}

/* Read a word at address p */
static inline uint64_t get_payload(void *p) {
    return (*(uint64_t *)p);  // Interpret memory at p as size_t
}
/* write a word at address p */
static inline void put_word(void *p, uint64_t val){
    (*(uint64_t *)p) = val;
}
/* Read the size and allocated fields from address p */
static inline size_t get_size(void *p){
    return (get_payload(p) & ~0xF);
}

static inline int get_alloc(void *p){
    return (get_payload(p)) & 0x1;
}

/* Given block ptr bp, compute address of its header and footer */
static inline void *hdr_pointer(void *bp){
    void *hdr = (char *)bp - WSIZE;
    return hdr;
}
static inline void *ftr_pointer(void *bp){
    return (char *)bp + get_size(hdr_pointer(bp)) - DSIZE;
    
}

/* Given block ptr bp, compute address of next and previous blocks */
static inline void *next_blkp(void *bp){
   return  bp + get_size(hdr_pointer(bp));
}

static inline void *prev_blkp(void *bp){
    return bp - get_size(bp - DSIZE);
}
/* Global variables */
static char *heap_listp = NULL;  /* Pointer to first block */

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
    mem_init();
   
    /* Create the initial empty heap with 32 bytes */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        perror("mem_sbrk");
        exit(1);
    }
    printf("initial heap address: %p\n", heap_listp);
    put_word(heap_listp, 0);                          /* Alignment padding */
    put_word(heap_listp + (1*WSIZE), pack(DSIZE, 1)); /* Prologue header */
    printf("prologue header address: %p\n", hdr_pointer(heap_listp));
    put_word(heap_listp + (2*WSIZE), pack(DSIZE, 1)); /* Prologue footer */
    printf("prologue footer address: %p\n", ftr_pointer(heap_listp));
    put_word(heap_listp + (3*WSIZE), pack(0, 1));     /* Epilogue header */
    printf("epilogue header address: %p\n", hdr_pointer(heap_listp));
    heap_listp += (2*WSIZE);
    printf("address: %p\n", heap_listp);
    

    /* Extend the empty heap with a free block of CHUNKSIZE bytes
    I use the right shift to avoid divide by zero error incase of bugs */
    if (extend_heap(CHUNKSIZE >> 3) == NULL) {
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
 * the mm_malloc returns a pointer to a block of memory(user payload request)
 * since this is a 64 bit mode implementation, the address requested 
 * should be a multiple of 16 according to my padding implementation
 * the runtime of the mm_maloc is linear in the number of free blocks.
 * for example if all newly free blocks are the minimum block size and a 
 * user request a payload of 24 bytes, the search will be linear in finding
 * a fit and placing the allocated blocks.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

   /*if the heap pointer is null, initialize a new page
   by calling extend heap
   */
    if (heap_listp == NULL){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* user cannot request more than the allowable memnory allow*/
    if (size > (CHUNKSIZE << 2))
        errno = ENOMEM;

    /* Adjust block size to include overhead and alignment reqs.
    * if the user payload is less than the minimum allowable size
    * the default is 32 bytes.
    *  */
    if (size <= DSIZE) // to many request of smaller payloads leads to internal fragmentation
        asize = MIN_BLOCK_SIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); 

    /* Search the free list for a fit  based on user request and place the newly alloacted block to
    * be return to the user. find fit is a strategy that places  smaller blocks in front of the implicit 
    * free list with larger blocks at the end */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block 
    * note that I am using right and left shifts for division to prevent divide by zero errors
    which enforce alignment*/
    extendsize = ((asize + CHUNKSIZE - 1) >> 12 ) <<  12;

    if ((bp = extend_heap(extendsize >> 3)) == NULL)
        return NULL;
    place(bp, asize);
    printf("Allocated block at: %p after extending heap\n", bp);  
    return bp;
}

/*
 * mm_free - Free an allocated block in memory. Each free request 
 * must correspond to a currently allocated block obtain from a previous allocated request.
 * the void *p is a pointer to the block in memory to be free which  has to be allocated first
 * the runtime of mm_free is constant in all cases.
 */
void mm_free(void *bp)
{
    if (bp == NULL)
        return;

    size_t size = get_size(hdr_pointer(bp));
    if (heap_listp == NULL){
        mm_init();
    }
    // if the newly free block is the only free block on the heap, this takes
    //care of case 1 for coalesce as it is marked as free without any adjacent free neighbors
    put_word(hdr_pointer(bp), pack(size, 0));
    put_word(hdr_pointer(bp), pack(size, 0));
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 * this implementation helps to merge splinter blocks athat are adjacent on
 * the heap. Main intent of coalescing is to combat false fragmentation, the implementation
 * use here is immediate coalescing as this function is called in mm_free
 */
static void *coalesce(void *current_bp)
{
    size_t prev_alloc = get_alloc(ftr_pointer(prev_blkp(current_bp)));
    size_t next_alloc = get_alloc(hdr_pointer(next_blkp(current_bp)));
    size_t current_block_size = get_size(hdr_pointer(current_bp));
    printf("Coalescing block at %p of size %zu\n", current_bp, current_block_size);  // Debug: Before coalescing


    if (prev_alloc && next_alloc) {    /* Case 1, no coalescing is possible as both adjacent blocks are allocated*/
        return current_bp;
    }
    /* Case 2, current block is merge with next block, the header of the current block  and the footer 
    of the next block */
    else if (prev_alloc && !next_alloc) {      
        current_block_size += get_size(hdr_pointer(next_blkp(current_bp)));
        put_word(hdr_pointer(current_bp), pack(current_block_size, 0));
        put_word(ftr_pointer(current_bp), pack(current_block_size,0));
       
    }
    else if (!prev_alloc && next_alloc) {      /* Case 3, cost of merge */
        current_block_size += get_size(hdr_pointer(prev_blkp(current_bp)));
        put_word(ftr_pointer(current_bp), pack(current_block_size, 0));
        put_word(hdr_pointer(prev_blkp(current_bp)), pack(current_block_size, 0));
        current_bp = prev_blkp(current_bp);
    }
    else {                                     /* Case 4 */
        current_block_size += get_size(hdr_pointer(prev_blkp(current_bp))) +
            get_size(ftr_pointer(next_blkp(current_bp)));
        put_word(hdr_pointer(prev_blkp(current_bp)), pack(current_block_size, 0));
        put_word(ftr_pointer(next_blkp(current_bp)), pack(current_block_size, 0));
        current_bp = prev_blkp(current_bp);
    }
    //printblock(current_bp);
    //printf("After coalescing, block at %p of size %zu\n", current_bp, current_block_size);
    return current_bp;
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
    oldsize = get_size(hdr_pointer(ptr));
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
    if ((uintptr_t)(bp = mem_sbrk(size)) == -1)
        return NULL;

    //printf("extended heap by %zu bytes, new block address: %p\n", size, bp);
    //printf("next block address: %p\n", hdr_pointer(next_blkp(bp)));

    /* Initialize free block header/footer and the epilogue header */
    put_word(hdr_pointer(bp), pack(size, 0));         /* Free block header */
    put_word(ftr_pointer(bp), pack(size, 0));         /* Free block footer */
    put_word(hdr_pointer(next_blkp(bp)), pack(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 * and split if remainder would be at least minimum block size
 * we place the blocks and the remainder is moved to the next free block
 * the spliiting of free blocks causes internal fragmentation and can be comparable to the 
 * same as the user keeps requesting smaller size blocks.  If we have a large block at the beginning
 * of the heap and the user request a smaller size, we split the allocated block and set the
 * rest as a free block on the heap
 * 
 */
static void place(void *bp, size_t asize)
{
    size_t current_size = get_size(hdr_pointer(bp));
    printf("Placing block of size %zu at %p\n", asize, bp);

    if ((current_size - asize) >= (MIN_BLOCK_SIZE)) {
        put_word(hdr_pointer(bp), pack(asize, 1));
        put_word(ftr_pointer(bp), pack(asize, 1));

        bp = next_blkp(bp);
        put_word(hdr_pointer(bp), pack(current_size-asize, 0));
        put_word(ftr_pointer(bp), pack(current_size-asize, 0));
    }
    else {
        //we do nothing and set the current block as allocated
        put_word(hdr_pointer(bp), pack(current_size, 1));
        put_word(ftr_pointer(bp), pack(current_size, 1));
    }
}

/*
 * The placement policy being used in the find fit function is the
 * first fit. The first fit placement policy can be efficient if user request in payload
 * is always within the size of newly free blocks, however in this placement policy, all larger
 * blocks are at the end of the list, which can be linear in search but can have a best case of constatnt 
 * search if request for smaller blocks are made as smaller blocks are at the beggining of the list.
 * the cost of searching for a free block where there are splinters block can be linear in worst case
 */
static void *find_fit(size_t asize)
{
    /* First-fit search is a linear search with runtime 0(N)*/
    void *bp;
    for (bp = heap_listp; get_size(hdr_pointer(bp)) > 0; bp = next_blkp(bp)) {
        if (!get_alloc(hdr_pointer(bp)) && (asize <= get_size(hdr_pointer(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

void test_heap_traversal(void *bp) {
    bp = heap_listp;
    while (get_size(hdr_pointer(bp)) > 0) {
        size_t size = get_size(hdr_pointer(bp));
        int alloc = get_alloc(hdr_pointer(bp));
        bp = next_blkp(bp);
    }
}

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = get_size(hdr_pointer(bp));
    halloc = get_alloc(hdr_pointer(bp));
    fsize = get_size(ftr_pointer(bp));
    falloc = get_alloc(ftr_pointer(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }
    
    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
    test_heap_traversal(bp);
}
void debug_block(void *bp) {
    size_t hsize = get_size(hdr_pointer(bp));
    size_t halloc = get_alloc(hdr_pointer(bp));
    size_t size  = get_size(ftr_pointer(bp));
    size_t  falloc = get_alloc(ftr_pointer(bp));
    printf("Blockss at %p: header: [%ld:%c]\n", bp, hsize, (halloc ? 'a' : 'f'));
    if (!halloc) {
        printf("Free block footer at %p: [%ld:%c]\n",bp, size, (halloc ? 'a' : 'f'));
    }
    test_heap_traversal(bp);
}

static void print_blocks(void *bp){
    bp = heap_listp;
    while(bp != NULL){
        printblock(bp);
        bp = next_blkp(bp);
        if(get_size(hdr_pointer(bp)) == 0){
            break;
        }
    }
}

static void checkblock(void *bp)
{
    size_t hsize = get_size(hdr_pointer(bp));
    if ((size_t)bp % DSIZE != 0)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (get_payload(hdr_pointer(bp)) != get_payload(ftr_pointer(bp)))
        printf("Error: header does not match footer\n");
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((get_size(hdr_pointer(heap_listp)) != DSIZE) || !get_alloc(hdr_pointer(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; get_size(hdr_pointer(bp)) > 0; bp = next_blkp(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((get_size(hdr_pointer(bp)) != 0) || !(get_alloc(hdr_pointer(bp))))
        printf("Bad epilogue header\n");
}

