#ifndef _GREPTILE_H
#define _GREPTILE_H
#define _POSIX_C_SOURCE 200809L
#if defined(SOLARIS)
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 700
#endif

#define REGULAR_FILE   1       /* file other than directory */
#define DIRECTORY   2       /* directory */
#define NO_PERM 3
#define CANT_STAT 4
#define PATHMAX 2048
#define MAX_LINE 30
#define MAX_FILES 1024
#define NUM_THREADS 4


#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"
#define MAXLINE 4096


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>


// Initialized in main() based on command-line arguments
static char *fullpath;
struct stat buff;
size_t pattern_len;
size_t file_print_offset;
pthread_t ntid;
long regid;
long dirid;
int jobid;
char *filename;


/*initialize a search job*/
struct search_job {
    char *file_path; /*filepath of job, only regular files*/
    off_t file_size; /*filesize of job*/
};


/*initialize a search ring buffer with a copy of jobs*/
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







void rb_init(struct search_ring_buffer *rb, size_t capacity);
void rb_destroy(struct search_ring_buffer *rb);
bool rb_empty(struct search_ring_buffer *rb);
bool rb_full(struct search_ring_buffer *rb);
void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size);
struct search_job rb_dequeue(struct search_ring_buffer *rb);


void *search_files(void *arg);
void traverse_directory(const char *path);


static void err_doit(int errnoflag, int error, const char *fmt, va_list ap);
void err_ret(const char *fmt, ...);
void err_sys(const char *fmt, ...);
void err_cont(int error, const char *fmt, ...);
void err_exit(int error, const char *fmt, ...);
void err_dump(const char *fmt, ...);
void err_msg(const char *fmt, ...);
void err_quit(const char *fmt, ...);

#endif  /* _GREPTILE_H */