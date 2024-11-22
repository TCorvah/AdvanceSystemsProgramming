#ifndef __MM_H__
#define __MM_H__
#include <stddef.h>

void mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *p);
void mm_deinit(void);

#endif
