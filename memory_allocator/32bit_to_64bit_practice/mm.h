#include <stddef.h> 
extern void mm_init(void);
extern void mm_deinit(void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern void mm_checkheap(int verbose);


