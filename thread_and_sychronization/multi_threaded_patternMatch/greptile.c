#include "greptile.h"
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>


// Initialized in main() based on command-line arguments
struct stat buff;
size_t pattern_len;
size_t file_print_offset;
int colorize;
struct search_ring_buffer search_rb;

// initialize the circular ring buffer
void rb_init(struct search_ring_buffer *rb, size_t capacity) {
    rb->jobs = malloc(capacity * sizeof(struct search_job));
    if (!rb->jobs)
        err_msg("malloc() failed");
    rb->capacity = capacity;
    rb->num_jobs = 0;
    rb->enqueue_index = 0;
    rb->dequeue_index = 0;
    if (pthread_mutex_init(&rb->mutex, NULL) != 0)
        perror("pthread_mutex_init() failed");
    if (pthread_cond_init(&rb->has_job_cond, NULL) != 0)
        perror("pthread_cond_init() failed");
    if (pthread_cond_init(&rb->has_space_cond, NULL) != 0)
        perror("pthread_cond_init() failed");
}

// frees the buffer
void rb_destroy(struct search_ring_buffer *rb) {
    if (rb->jobs) {
        free(rb->jobs);  // Free the allocated memory for jobs
    }
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->has_job_cond);
    pthread_cond_destroy(&rb->has_space_cond);
}

// checks status of the buffer if empty
bool rb_empty(struct search_ring_buffer *rb){
    return(rb->num_jobs == 0);
}

// checks status of the buffer if full
bool rb_full(struct search_ring_buffer *rb){
    return(rb->capacity == rb->num_jobs);
}

// this function enqueue a job into the ring buffer taking into account its
//size
void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size) { 
    struct search_job *job;

    if ((job = malloc(sizeof(struct search_job))) == NULL)
        perror("malloc failed");

    // Dynamically allocate memory for file_path and copy the string

    char *path = malloc(job->file_size);  // +1 for the null terminator
    if (job->file_path == NULL)
        perror("malloc failed for file_path");
       
   // lock the mutex
    pthread_mutex_lock(&rb->mutex);
    while(rb_full(rb)){
         pthread_cond_wait(&rb->has_space_cond, &rb->mutex);
    }

    assert(rb->num_jobs < rb->capacity); // Ensure buffer size is not exceeded

    // Insert the job into the buffer
    rb->jobs[rb->enqueue_index] = *job;

    //Update the enqueue_index to point to the next free slot
    rb->dequeue_index = (rb->dequeue_index + 1) % rb->capacity; 
    rb->num_jobs++;
    printf(" new job added to buffer %s\n",file_path );
    // Signal helps prevent interleave between the threads, so
    // that a single thread is working on a file at a time
    pthread_cond_signal(&rb->has_job_cond);  
    pthread_mutex_unlock(&rb->mutex);
    
}

struct search_job rb_dequeue(struct search_ring_buffer *rb) {
    printf("Dequeue attempt\n");
    struct search_job job;
    pthread_mutex_lock(&rb->mutex);

    // Wait until there is a job to dequeue
    while(rb_empty(rb)){
        pthread_cond_wait(&rb->has_job_cond, &rb->mutex);
    }
    // Dequeue the job from the buffer
    job = rb->jobs[rb->dequeue_index]; // Access the job at the dequeue index
    
    // Update the dequeue index to point to the next job
    rb->dequeue_index = (rb->dequeue_index + 1) % rb->capacity;
    rb->num_jobs--; // Decrease the number of jobs in the buffer

    // Signal that there is now space in the buffer
    pthread_cond_signal(&rb->has_space_cond);

    pthread_mutex_unlock(&rb->mutex);
    
    return job;
}
/* an array of file extensions for pattern search*/
const char *allowed_extensions[] = {".c", ".cpp", ".h", ".py", ".txt", ".md"};

const int num_allowed_extensions = sizeof(allowed_extensions) / sizeof(allowed_extensions[0]);

/* the following function takes in a filename and check to ensure only the allowable
* file extensions are consider in the search. The time to determine a file extension is linear
* in the length of the allowed extensions. 
*/
static int has_allowed_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');  // Find the last dot in the filename
    if (!dot || dot == filename) {
        return 0;  // No extension found, 
    }
    for (int i = 0; i < num_allowed_extensions; i++) {
        if (strcmp(dot, allowed_extensions[i]) == 0) {
            return 1;  // Extension matches one of the allowed
        }
    }
    return 0;  // Extension not in allowed list
}

void pq_init(struct print_queue *pq, char *file_path) {
    pq->file_path = file_path;
    pq->head = NULL;
    pq->tail = NULL;
}

void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num) {
    struct print_job *job = malloc(sizeof(struct print_job));
    if (!job)
        perror("malloc() failed");

    job->line = line;
    job->match = match;
    job->line_num = line_num;
    job->next = NULL;

    if (pq->head == NULL) {
        pq->head = job;
        pq->tail = job;
    } else {
        pq->tail->next = job;
        pq->tail = job;
    }
}

struct print_job *pq_pop_front(struct print_queue *pq) {
    if (pq->head == NULL)
        return NULL;

    struct print_job *job = pq->head;
    pq->head = job->next;

    return job;
}

void pq_print(struct print_queue *pq, const char *pattern) {
    struct print_job *job;

    if (colorize)
        printf(COLOR_MAGENTA "%s\n" COLOR_RESET, pq->file_path + file_print_offset);
    else
        printf("%s\n", pq->file_path + file_print_offset);

    while ((job = pq_pop_front(pq)) != NULL) {
        int match_offset = job->match - job->line; // Must be int to work with %.*s

        if (colorize) {
            printf(COLOR_GREEN "%d" COLOR_RESET ":%.*s" COLOR_RED "%s" COLOR_RESET "%s\n",
                job->line_num,
                match_offset, job->line,
                pattern,
                job->match + pattern_len);
        } else {
            printf("%d:%s\n", job->line_num, job->line);
        }

        free(job);
    }
}

// I am changing the semantics from fopen to open and others
// read function to avoid concurrent access of worker threads 
// in a multi threaded enviroment
char *read_file(char *file, size_t file_size){
    struct stat buff;
    int fd = 0;
    size_t contents_size = buff.st_size;

    // Get the size of the file
    if (lstat(file, &buff) < 0) {
        perror("stat() failed");
        return NULL;
    }
    if(!S_ISREG(buff.st_mode)){
        printf("Error: %s is not a regular file\n", file);
        return NULL;
    }

    //open file for readonly
    fd = open(file, O_RDONLY );
        if(fd == -1){
           perror("can't open file");
           close(fd);
           return NULL;
        }

    contents_size = buff.st_size > 0 ? buff.st_size : 1024;   
    char *path = malloc(contents_size);
    if(path == NULL){
        perror("mallaoc failed");
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd,path,file_size );
    if(bytes_read < 0){
        perror("read() failed");
        free(path);
        close(fd);
        return NULL;
    }
   
    close(fd);   
    return path;

}

void *worker_thread(void *arg){
    
    // initialize the ring buffer
    struct worker_args *args = (struct worker_args *)arg;

    struct search_ring_buffer *ring = args->ring;

    const char *pattern = arg;
    struct search_job job;
    struct print_queue queue;

    while(1){ 
        pthread_mutex_lock(&ring->mutex);
        while (rb_empty(&search_rb)) {
            pthread_cond_wait(&ring->has_job_cond, &ring->mutex);
        }
        printf("Worker thread started\n");
        // Dequeue a job from the ring buffer
        job = rb_dequeue(&search_rb);
          if (job.file_path == NULL) {
            pthread_mutex_unlock(&ring->mutex);
            break;  // Exit when NULL job is dequeued
        }
        off_t file_size = job.file_size;
        char *path = job.file_path;
        if(path == NULL){
            perror("invalid");
            return NULL;
        }
         
        printf("did we get here\n");

        // Read the file 
        char *read_files = read_file(job.file_path, file_size);
        if (!read_files) {
            perror("reading file into buffer failed");
            return NULL;
        }

        read_files[file_size] = '\0';

        struct print_queue pq;
        pq_init(&queue, job.file_path);

        char *line = read_files;
        int line_number = 1;
         while (line) {
            char *next_line = strchr(line, '\n');
            if (next_line) {
                *next_line = '\0'; // Temporarily null-terminate the current line
        }
        char *match = strstr(read_files, line);
            if(match){
              pq_add_tail(&pq, line, match, line_number);
              printf("Match found in file %s at line %d: %s\n", match, line_number, line);  

        }
         if (next_line) {
            *next_line = '\n'; // Restore newline character
            line = next_line + 1;
        } else {
            line = NULL;
        }
        line_number++;
    
    }
    if(pq.head != NULL){
        flockfile(stdout);
        pq_print(&pq, pattern);
        funlockfile(stdout);
    }

    // After processing the job, no need to reassign the file path
    printf("worker threads ready: received job %s",job.file_path );

    free(read_files);

    // Free the file path memory if it was dynamically allocated
    if (job.file_path) {
            free(job.file_path);  // Only free if it was dynamically allocated
        }
        pthread_mutex_unlock(&ring->mutex);
    }
    return NULL;
    
}
void free_buffer(char *buf, size_t file_size) {
    free(buf);
}

void *search_files(void *arg) {
    const char *pattern = arg;
    uint64_t found_match = 0;
    for (;;) {
        struct search_job job = rb_dequeue(&search_rb);

        off_t file_size = job.file_size;
        char *file_path = job.file_path;
        if (file_path == NULL)
            pthread_exit((void *)found_match);

        char *buf = read_file(file_path, file_size);
        if (!buf) {
            perror("reading file into buffer failed");
            break;  // Replace goto with break
        }

        buf[file_size] = '\0';

        struct print_queue pq;
        pq_init(&pq, file_path);

        char *next_line = buf;
        char *line = NULL;
        char *saveptr = NULL;  // Save pointer for strtok_r

        line = strtok_r(next_line, "\n", &saveptr);

        int line_num = 1;
        while (line != NULL) {
            char *match = strstr(line, pattern);
            if (match) {
                pq_add_tail(&pq, line, match, line_num);
                found_match = 1;
            }

            line = strtok_r(NULL, "\n", &saveptr);  // Thread-safe
            line_num++;
        }

        if (pq.head != NULL) {
            flockfile(stdout);
            pq_print(&pq, pattern);
            funlockfile(stdout);
        }

        free(buf);
        free(file_path);  // Always free file_path at the end of the loop
    }

    return NULL;  // Ensure the thread exits correctly if the loop ends
}


void create_worker_threads(int num_threads, struct search_ring_buffer *ring, const char *pattern) {
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, (void *)ring);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}
        
void traverse_directory(const char *path) {
    struct dirent *dirp;
    DIR *dp;
    struct stat statbuf;
    const char *match;

    if(lstat(path, &statbuf) < 0){
        perror("cant stat");
        return;
    }

    // Construct the full path of the directory
    if((dp = opendir(path)) == NULL){
       err_msg("can't open");
       return;
    }

    while ((dirp = readdir(dp)) != NULL){              
       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;
        // Allocate enough space for the path, a slash, the entry name, and a null byte
        size_t path_size = (strlen(path) + 1 + strlen(dirp->d_name) + 1) * sizeof(char);
        char *fullpath = malloc(path_size);
         if (!fullpath){
            perror("malloc() failed");
            continue;
         }
        // Construct the full path of the file/directory
        snprintf(fullpath, path_size, "%s/%s", path, dirp->d_name);

        if (lstat(fullpath, &statbuf) == -1) {
            perror("lstat");
            free(fullpath);
            continue;
        }
        if(S_ISDIR(statbuf.st_mode)){
            traverse_directory(fullpath);
            free(fullpath);
        }
        else if (S_ISREG(statbuf.st_mode)) {
                printf("Checking file: %s\n", fullpath);
            if (has_allowed_extension(fullpath)) {
                printf("Adding file to queue: %s\n", fullpath);
                rb_enqueue(&search_rb, fullpath, statbuf.st_size);
                printf("contents %s\n", fullpath);
        } else {
                printf("Skipping file (invalid extension): %s\n", fullpath);
               
        }
         free(fullpath);
        }

    }
        closedir(dp);
}
// Worker thread function prototype
void *worker_thread(void *arg);
void *search_files(void *ard);

int main(int argc, char **argv){
    int i, n;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <pattern>\n", argv[0]);
        return 1;
    
    } 
    // char *directory_path = argv[1];  // Can be "."
    // char *pattern = argv[2];
    const char *directory_path = argv[1];
    const char *pattern = argv[2];

    //uint64_t any_threads_matched = 0;

    rb_init(&search_rb, MAX_FILES);

   

    traverse_directory(directory_path);

    create_worker_threads(NUM_THREADS, &search_rb, pattern);

  

    rb_destroy(&search_rb);

   return 0;
}

