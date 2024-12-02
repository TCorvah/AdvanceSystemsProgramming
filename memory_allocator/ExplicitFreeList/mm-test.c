#include <stdio.h>
#include <stdlib.h>

#include "mm.h"

int main(int argc, char **argv)
{
    mm_init();
    mm_checkheap(1);

    char *p = mm_malloc(8);
    char *q = mm_malloc(1024);
    if (!p || !q) {
        perror("mm_malloc");
        exit(1);
    }

    fprintf(stderr, "p=%p q=%p\n", p, q);
    *p = *q = 'A';

    mm_checkheap(1);
    mm_free(p);
    mm_checkheap(1);
    mm_free(q);
    mm_checkheap(1);
    mm_deinit();
}
