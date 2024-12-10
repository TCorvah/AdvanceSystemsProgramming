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
 * free virtual memory from the heap.
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

#define ALIGNMENT 16 
#define WSIZE       8       /* Word and header/footer size (bytes) */
#define DWORD_SIZE  16      /* Double-word size (bytes) */
#define CHUNKSIZE   4096    /* Extend heap by this amount (bytes) */
#define ALIGN(size) (((size) + (0XF)) & ~(0XF)) /* for 32 byte alignment */
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
    return  (header_t *)((char *)payload - WSIZE);    
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
    return header(payload)->links.fprev->payload;
}

static inline int floor_log2(uint64_t num) {
    return 64 - __builtin_clzl(num) - 1;
}

/* Global pointer to the start of the heap, (char *) 
is store as 8 byte quad word on a x86-64  machine*/
static char *heap_listp = NULL; 

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
    // 
    assert(sizeof(header_t) == 3 * WSIZE);
    //  should be WORD_SIZE bytes after the beginning of a header
    assert(offsetof(header_t, payload) == WSIZE);
    // assert footer is 8 bytes
    assert(sizeof(footer_t) == WSIZE);


    mem_init();

    /* 
     * Create the initial empty heap. The heap is initialized with a total of 48 bytes, 
     * structured as follows:
     *   - 32 bytes for the prologue block, consisting of:
     *       - 8 bytes for the header
     *       - 8 bytes for the 'next' pointer
     *       - 8 bytes for the 'previous' pointer
     *       - 8 bytes for the footer
     *   - 8 bytes for the epilogue block (header only).
     *   - 8 bytes for the initial heap pointer, which starts just after the prologue 
     *     block but before the epilogue header.
     */

    size_t init_size = 2 * WSIZE + ( (WSIZE * 4));
    if ((heap_listp = mem_sbrk( init_size) ) == (void *)-1) {
        perror("mem_sbrk");
        exit(1);
    }
    // 8-byte leading padding
    memset(heap_listp, 0, WSIZE);

    heap_listp += DWORD_SIZE;

    /*The prologue block is the only allocated block with a previous and 
    * next pointer, its implementation is for alignment purposes
    */ 
    header_t *prologue_hdr = header(heap_listp);
    prologue_hdr->size = ALIGN(2 * DWORD_SIZE);
    prologue_hdr->allocated = 1;
    prologue_hdr->links.fprev = prologue_hdr->links.fnext = prologue_hdr;
    footer_t *prologue_ftr = footer(heap_listp);
    prologue_ftr->size = ALIGN(2 * DWORD_SIZE);
    prologue_ftr->allocated = 1;

 
    // Initialize epilogue block 
    heap_listp +=  2 * DWORD_SIZE;
    header_t *epilogue_hdr = header(heap_listp);
    epilogue_hdr->size = 0;
    epilogue_hdr->allocated = 1;
  

    heap_listp = (char *)heap_listp -  (2 * DWORD_SIZE);
    
    // Extend the empty heap with a free block of PAGE_SIZE bytes
    if (extend_heap(CHUNKSIZE >> 4) == NULL) {
        perror("extend_heap");
       exit(1);
    }
 

}

void mm_deinit(void)
{
    mem_deinit();
}

/*
 * mm_malloc - Allocate a block with at least 'size' bytes of payload.
 * The minimum block size is 32 bytes (4 words), consisting of:
 *   - 16 bytes for payload
 *   - 8 bytes for the header
 *   - 8 bytes for the footer.
 * 
 * All data returned by malloc must be 16-byte aligned, including the block address, 
 * to comply with 64-bit machine alignment requirements. 
 * 
 * Since the heap grows upwards, the block addresses must increase monotonically to ensure correctness.
 * 
 * Allocation examples:
 *   - If a user requests 8 bytes, we round it up to 16 bytes (for payload) and add 8 bytes each for 
 *     the header and footer, resulting in a total block size of 32 bytes.
 *   - If a user requests 20 bytes, we round it up to the nearest multiple of 16, which is 32 bytes 
 *     (payload), and add the header and footer, resulting in a total block size of 48 bytes.
 * 
 * In this explicit free list implementation:
 *   - The free list is traversed to find a suitable block that has been recently freed.
 *   - Newly freed blocks are inserted at the head of the free list, which allows for efficient 
 *     allocation of recently freed memory.
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
    /*
     constraint for max size a user can request
    */
     if (size >  (CHUNKSIZE * 2)){ 
        errno = ENOMEM;
     }
    /* 
     * Adjust the block size to include overhead and alignment requirements. 
     * Note that this condition can lead to internal fragmentation. 
     * For example, if a user repeatedly requests small payloads, the allocator 
     * will add padding to meet alignment requirements. The unused space in each 
     * block, caused by the padding, contributes to fragmentation within the heap.
     */
    if (size <= DWORD_SIZE){ 
        asize = 2*DWORD_SIZE; 
    }else{
        asize = DWORD_SIZE * ((size + (DWORD_SIZE) + (DWORD_SIZE-1)) / DWORD_SIZE); 
    }
    /* 
     * Search the free list for a fit using the first fit placement policy. Note that there may be many small free blocks 
     * that could collectively satisfy a user's request if they were contiguous in memory. 
     * External fragmentation occurs when a user requests a block size, but no suitable 
     * contiguous block is found, even though there is sufficient free memory overall. 
     * Coalescing helps mitigate this issue by merging adjacent free blocks into larger 
     * contiguous blocks.
     */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found even with the coalesce, we extend the heap by a chunksize of memory 
    * and place the remaining block in the explicit free list
   */
    extendsize = ((asize + CHUNKSIZE - 1) >> 12 ) <<  12;
    if ((bp = extend_heap(extendsize >> 4)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Frees a block of memory and adds it to the free list, 
 * which keeps track of available blocks on the heap. The actual 
 * merging of adjacent free blocks (if applicable) is handled 
 * by the coalesce function, which performs the heavy lifting.
 */
void mm_free(void *bp)
{
    
    // check if the block pointer is null
    if (bp == NULL) {
        return;
    }
    
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Returns a pointer to the coalesced block.
 * This function merges adjacent free blocks to reduce external fragmentation 
 * and improve the likelihood of finding contiguous free memory. 
 * Newly merged free blocks are added to the front of the free list for 
 * quick access and removed from their previous positions in the list.
 */
static void *coalesce(void *current_block)
{
    void *prev = prev_payload(current_block);
    void *next = next_payload(current_block);

    size_t prev_alloc = header(prev)->allocated;
    size_t next_alloc = header(next)->allocated;

    size_t current_payload_size = header(current_block)->size; 

    if (prev_alloc == 1 && next_alloc == 1) {
        /* Case 1: No coalescing needed, both previous and next blocks are allocated */
        header_t *new_free_hdr = header(current_block);
        footer_t *new_free_ftr = footer(current_block);
        new_free_hdr->size = current_payload_size;
        new_free_ftr->size = current_payload_size;
        new_free_hdr->allocated = new_free_ftr->allocated = 0;
        new_free_hdr->links.fprev = new_free_hdr->links.fnext = new_free_hdr;
        add_merge_block_to_freelist(current_block);
        
    } else if (prev_alloc == 1 && next_alloc == 0) {
        /* Case 2: Coalesce with the next block, previous block is allocated */
        current_payload_size += header(next)->size;
        header(current_block)->size = ALIGN(current_payload_size);
        footer(current_block)->size = ALIGN(current_payload_size);
        footer(current_block)->allocated = 0;

        // Remove next from the free list
        remove_from_freelist(next);

    } else if (prev_alloc == 0 && next_alloc == 1) {
        /* Case 3: Coalesce with the previous block, next block is allocated */
        current_payload_size += header(prev)->size;
        footer(current_block)->size = ALIGN(current_payload_size);
        header(prev)->size = ALIGN(current_payload_size);
        header(prev)->allocated = 0;


        // Remove prev from the free list
        remove_from_freelist(prev);

        // Coalesced free block starts at prev now
        current_block = prev;
    } else {
        /* Case 4: Coalesce with both previous and next blocks */
        current_payload_size += header(prev)->size + footer(next)->size;
        header(prev)->size = ALIGN(current_payload_size);
        footer(next)->size = ALIGN(current_payload_size);

 
        // Remove prev and next from free list
        remove_from_freelist(prev);
        remove_from_freelist(next);
        // Coalesced free block starts at prev now
        current_block = prev;
    }
    // Add coalesced block to beginning of free list
    add_merge_block_to_freelist(current_block);
    
    return current_block;
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
     using a word*/
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if (size < (8 * WSIZE))
    {
        size = 8 * WSIZE;
    }
    
    if ((uintptr_t)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    header(bp)->size = size;        /* Free block header */
    footer(bp)->size = size;
    header(bp)->allocated = footer(bp)->allocated = 0;
    
    header(bp)->links.fprev =  header(bp)->links.fnext = header(bp);
   
     /* New epilogue header */
    header(next_payload(bp))->size = 0;
    header(next_payload(bp))->allocated = 1; 
   
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes in front pf the explicit
 *  free list and split if remainder would be at least minimum block size
 *  this technique can sometimes introduce internal fragmentation.
 *  
 */
static void place(void *p, size_t asize)
{
    size_t current_size = header(p)->size;

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
        //coalesce(q);

        // Add q to the free list in p's place
        header(fnext_p)->links.fprev = header(fprev_p)->links.fnext = header(q);
        header(q)->links.fprev = header(fprev_p);
        header(q)->links.fnext = header(fnext_p);
   
        coalesce(q);
    } else {
        // There was no leftover, simply remove p from the free list
        header(fnext_p)->links.fprev = header(fprev_p);
        header(fprev_p)->links.fnext = header(fnext_p);
        header(p)->allocated = footer(p)->allocated = 1;
        remove_from_freelist(p);
       
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
    size_t hsize, halloc, fsize, falloc, plinks;

    hsize = header(p)->size;
    halloc = header(p)->allocated;
    fsize = footer(p)->size;
    falloc = footer(p)->allocated;
    void *a =  header(p)->links.fprev->payload;
    void *b =   header(p)->links.fnext->payload;

    // Print free list links for free blocks and the sentinel node
    // the sentinel node is the only allocated block with a previous
    // and next pointer, it implementation is soley for alignment 
    // purposes
    plinks = !halloc || p == heap_listp;

    if (hsize == 0) {
        printf("%p: EOL\n", p);
        return;
    }

    if (plinks) {
        printf("%p: header: [%lu:%c] {%p|%p} footer: [%lu:%c]\n", p,
            hsize, (halloc ? 'a' : 'f'),
               header(p)->links.fprev->payload,
               header(p)->links.fnext->payload,
               fsize, (falloc ? 'a' : 'f'));

    } else {
        printf("%p: header: [%lu:%c] {} footer: [%lu:%c]\n", p,
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
    if ((size_t)bp % DWORD_SIZE != 0)
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
    for (p = heap_listp; header(p)->size > 0; p = (void*)next_payload(p)) {
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
