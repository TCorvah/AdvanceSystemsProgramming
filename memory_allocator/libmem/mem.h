#ifndef __MEM_H__
#define __MEM_H__

void mem_init(void);
void *mem_sbrk(int incr);
void mem_deinit(void);

#endif
