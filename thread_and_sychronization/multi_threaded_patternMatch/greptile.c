#include "greptile.h"
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 1024
#define NUM_THREADS 4

#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"

// Initialized in main() based on command-line arguments
size_t pattern_len;
size_t file_print_offset;
int colorize;

static inline void error(char *msg) {
    perror(msg);
    exit(2);
}


static struct search_ring_buffer search_rb;

void rb_init(struct search_ring_buffer *rb, size_t capacity) {
    rb->jobs = malloc(capacity * sizeof(struct search_job));
    if (!rb->jobs)
        error("malloc() failed");

    rb->capacity = capacity;
    rb->num_jobs = 0;
    rb->enqueue_index = 0;
    rb->dequeue_index = 0;

    if (pthread_mutex_init(&rb->mutex, NULL) != 0)
        error("pthread_mutex_init() failed");
    if (pthread_cond_init(&rb->has_job_cond, NULL) != 0)
        error("pthread_cond_init() failed");
    if (pthread_cond_init(&rb->has_space_cond, NULL) != 0)
        error("pthread_cond_init() failed");
}

void rb_destroy(struct search_ring_buffer *rb) {
    free(rb->jobs);
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->has_job_cond);
    pthread_cond_destroy(&rb->has_space_cond);
}

void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size) {
    pthread_mutex_lock(&rb->mutex);

    // Wait until there's space in the ring buffer
    while (rb->num_jobs == rb->capacity)
        pthread_cond_wait(&rb->has_space_cond, &rb->mutex);

    rb->jobs[rb->enqueue_index] = (struct search_job){file_path, file_size};
    rb->enqueue_index = (rb->enqueue_index + 1) % rb->capacity;
    rb->num_jobs++;

    pthread_cond_signal(&rb->has_job_cond);
    pthread_mutex_unlock(&rb->mutex);
}

struct search_job rb_dequeue(struct search_ring_buffer *rb) {
    struct search_job job;
    pthread_mutex_lock(&rb->mutex);

    // Wait until the ring buffer is not empty
    while (rb->num_jobs == 0)
        pthread_cond_wait(&rb->has_job_cond, &rb->mutex);

    job = rb->jobs[rb->dequeue_index];
    rb->dequeue_index = (rb->dequeue_index + 1) % rb->capacity;
    rb->num_jobs--;

    pthread_cond_signal(&rb->has_space_cond);
    pthread_mutex_unlock(&rb->mutex);
    return job;
}

void pq_init(struct print_queue *pq, char *file_path) {
    pq->file_path = file_path;
    pq->head = NULL;
    pq->tail = NULL;
}

void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num) {
    struct print_job *job = malloc(sizeof(struct print_job));
    if (!job)
        error("malloc() failed");

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

// the following function reads the file into the buffer
// by line using fread
char *read_file_into_buffer(char *path, size_t file_size) {

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }   
    // Get the file descriptor from the FILE * using fileno
    int fd = fileno(file);
    if (fd == -1) {
        perror("Failed to get file descriptor");
        fclose(file);  // Close the file pointer
        return NULL;
    }

    // Use fstat to get file information
    struct stat file_info;
    if (fstat(fd, &file_info) == -1) {
        perror("Failed to get file stats");
        fclose(file);  // Close the file pointer
        return NULL;
    }

    size_t filesize = file_info.st_size;  // Get the file size from fstat

    // allocate memory on the heap for the file contents
    char *buf = malloc(filesize + 1);
    if (!buf)
        error("malloc() failed");
  
    size_t items_read = fread(buf, 1, filesize, file);
    if (items_read != file_size) {
        if (ferror(file)) {
            // Error occurred during the fread operation
            perror("Error reading file");
        } else if (feof(file)) {
        // End of file reached before reading the expected number of bytes
         fprintf(stderr, "Warning: End of file reached, only %zu bytes read out of %zu\n", items_read, file_size);
    }

    }

    fclose(file);
    return buf;
}

char *search_pattern_in_line(const char *line, const char *pattern) {
    return strstr(line, pattern);  // Returns pointer to the match or NULL if not found
}

// Returns void * for pthread_create() signature
void *search_files(void *arg) {
    const char *pattern = arg;
    uint64_t found_match = 0;

    while(1) {
        struct search_job job = rb_dequeue(&search_rb);

        off_t file_size = job.file_size;
        char *file_path = job.file_path;
        if (file_path == NULL)
            pthread_exit((void *)found_match);

        char *buf = read_file_into_buffer(file_path, file_size);
        if (!buf) {
            error("error");
       
        }

        buf[file_size] = '\0';

        struct print_queue pq;
        pq_init(&pq, file_path);
          // Process the file content line by line
        char *next_line = buf;
        char *line = strtok_r(next_line, "\n", &next_line); // Use strtok_r for thread-safety

        int line_num = 1;
        while (line) {
            char *match = search_pattern_in_line(line, pattern);
             // Returns pointer to the match or NULL if not found
            if (match) {
                pq_add_tail(&pq, line, match, line_num);
                found_match = 1;
            }

            line = strtok_r(next_line, "\n", &next_line); // Move to next line
            line_num++;
        }
        // Print matches
        if (pq.head != NULL) {
            flockfile(stdout);
            pq_print(&pq, pattern);
            funlockfile(stdout);
        }
        // free the path and the buffer
        free(buf);
        free(file_path);
    }
}

/* 
 * `tranverse_directory` handles different file types, increments counters for regular files 
 * and directories, and handles errors like permission denial or stat errors.
 */  
void traverse_directory(const char *path) {
    struct dirent *entry;
    DIR *dp;
    struct stat statbuf;

    if(lstat(path, &statbuf) < 0){
        error("cant stat");
    }
    // Construct the full path of the directory
    if((dp = opendir(path)) == NULL){
        error("can't open");
    }

    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Allocate enough space for the path, a slash, the entry name, and a null byte
        size_t path_size = (strlen(path) + 1 + strlen(entry->d_name) + 1) * sizeof(char);

        char *full_path = malloc(path_size);
        if (!full_path)
            error("malloc() failed");

        snprintf(full_path, path_size, "%s/%s", path, entry->d_name);

        if (lstat(full_path, &statbuf) == -1)
            error("lstat() failed");

        if (S_ISDIR(statbuf.st_mode)) {
            traverse_directory(full_path);
            free(full_path);
        } else if (S_ISREG(statbuf.st_mode) && statbuf.st_size != 0) {
            rb_enqueue(&search_rb, full_path, statbuf.st_size);
            // full_path will be freed by a worker thread
        } else {
            free(full_path);
        }
    }

    closedir(dp);
}


int main(int argc, char **argv) {
    char *directory_path = ".";
    char *pattern;

    if (argc == 2) {
        pattern = argv[1];
        file_print_offset = 2; // Skip "./" prefix if no directory argument
    } else if (argc == 3) {
        pattern = argv[1];
        directory_path = argv[2];
        file_print_offset = 0; // Print full path if directory argument is given
    } else {
        fprintf(stderr, "usage: greptile <pattern> [directory]\n");
        exit(2);
    }

    pattern_len = strlen(pattern);
    colorize = isatty(STDOUT_FILENO);
    uint64_t any_threads_matched = 0;

    rb_init(&search_rb, MAX_FILES);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, search_files, (void *)pattern);

    // main thread tranverse the directory and 
    traverse_directory(directory_path);
    for (int i = 0; i < NUM_THREADS; i++)
        rb_enqueue(&search_rb, NULL, 0);

    // Wait for worker threads to finish 
    for (int i = 0; i < NUM_THREADS; i++) {
        uint64_t thread_matched = 0;
        pthread_join(threads[i], (void **)&thread_matched);
        any_threads_matched |= thread_matched;
    }

    rb_destroy(&search_rb);

    // Return 0 if any thread found a match, 1 otherwise
    return any_threads_matched == 0;
}
