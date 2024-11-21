#include "greptile.h"
#include <sys/uio.h>

static struct search_ring_buffer search_rb;

int main(int argc, char **argv){
    char *directory_path = ".";
    char *pattern, *path;
    int i, n, err;
    pthread_t threads;
    struct print_queue *queue;

    if (argc == 2) {
        pattern = argv[1];
        file_print_offset = 2; // Skip "./" prefix if no directory argument
    } else if (argc == 3) {
        pattern = argv[1];
        directory_path = argv[2];
        file_print_offset = 0; // Print full path if directory argument is given
    } else {
        err_quit("usage: greptile <pattern> [directory]\n");
    }
    
    path = realpath(directory_path, NULL);
    pattern_len = strlen(pattern);
    file_print_offset = strlen(directory_path);
    if( file_print_offset < 0)
        file_print_offset = PATH_MAX;
    if((directory_path = malloc(file_print_offset)) == NULL)
       err_sys("malloc error");
    if(pattern == NULL)
       err_quit("need pattern");
       exit(1);


    uint64_t any_threads_matched = 0;

    rb_init(&search_rb, MAX_FILES);
    pq_init(queue, path);
    
   err = pthread_create(&threads, NULL, p )
    // Complete the implementation of this function
    printf("total: %d\n", search_rb.num_jobs);
 
    rb_destroy(&search_rb);

    // Return 0 if any thread found a match, 1 otherwise
    //return any_threads_matched == 0;
   return 0;
}