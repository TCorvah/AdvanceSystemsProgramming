#ifndef _GREPTILE_H
#define _GREPTILE_H

#define _POSIX_C_SOURCE 200809L
#if defined(SOLARIS)
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 700
#endif

#define PATHMAX 2048
#define MAX_LINE 30
#define MAX_FILES 1024
#define NUM_THREADS 4
#define MAXLINE 4096


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>

/*initialize a search job*/
struct search_job {
    char *file_path; /* File path for the job (read-only) */
    off_t file_size; /*filesize of job*/
};


// Ring buffer for thread communication
struct search_ring_buffer {
    struct search_job *jobs;  /*copies of jobs in ring buffer*/
    size_t capacity;     /*capacity or max length of ring buffer*/
    int num_jobs;       /*number of jobs in ring buffer*/
    int enqueue_index;  /*write index of ring buffer*/
    int dequeue_index; /*read index of ring buffer*/
    pthread_mutex_t mutex; /*mutex to protect jobs*/
    pthread_cond_t has_job_cond; /*conditional mutex for removing jobs*/
    pthread_cond_t has_space_cond; /*conditional mutex for adding jobs*/
};

// when a job is successfully dequeud from a ring buffer, 
struct print_job {
    char *line; // line where match is found
    char *match; // points to the match within line and color match
    int line_num; // line number where the match is found
    struct print_job *next; //
};
// prints all job in a queue in a FIFO manner
struct print_queue {
    char *file_path;
    struct print_job *head;
    struct print_job *tail;
};


void rb_init(struct search_ring_buffer *rb, size_t capacity);
void rb_destroy(struct search_ring_buffer *rb);
bool rb_empty(struct search_ring_buffer *rb);
bool rb_full(struct search_ring_buffer *rb);
void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size);
struct search_job rb_dequeue(struct search_ring_buffer *rb);
void traverse_directory(const char *path);


void err_cont(int error, const char *fmt, ...);
void err_exit(int error, const char *fmt, ...);
void err_dump(const char *fmt, ...);
void err_msg(const char *fmt, ...);
void err_quit(const char *fmt, ...);

#endif  /* _GREPTILE_H */