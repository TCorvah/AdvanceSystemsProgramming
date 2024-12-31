#include "greptile.h"
#include "myQueue.h"
#include <dirent.h>
#include <limits.h>
#include <errno.h>

// Initialized in main() based on command-line arguments
static char *fullpath;
struct stat buff;
size_t pattern_len;
size_t file_print_offset;
char *filename;

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


void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size) { 
    struct search_job *job;

    if ((job = malloc(sizeof(struct search_job))) == NULL)
        err_sys("malloc failed");

    // Dynamically allocate memory for file_path and copy the string
    job->file_path = malloc(strlen(file_path) + 1);  // +1 for the null terminator
    if (job->file_path == NULL)
        err_sys("malloc failed for file_path");

    strcpy(job->file_path, file_path);  // Copy file_path string
       
   
    job->file_size = file_size;

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

    if(rb->enqueue_index == rb->dequeue_index)
       rb->enqueue_index = (rb->dequeue_index + 1) % rb->capacity;

    printf(" new job added to buffer %s\n",file_path );

    // Signal that there is a job available in the buffer
    pthread_cond_broadcast(&rb->has_job_cond);  
    pthread_mutex_unlock(&rb->mutex);
    
}

struct search_job rb_dequeue(struct search_ring_buffer *rb) {
    struct search_job job;
    // Implement this function
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


void read_file(char *file, const char *pattern){
    struct stat buff;
    FILE *fptr;
    char *match;
    //open file for readonly

    fptr = fopen(file, "r" );
        if(fptr == NULL){
           perror("can't open file");
           return;
        }

    // Get the size of the file
    if (lstat(file, &buff) < 0) {
        perror("stat() failed");
        fclose(fptr);
        return;
    }
 
    char *path = malloc(buff.st_size + 1);
    if(path == NULL){
        perror("mallaoc failed");
        fclose(fptr);
        return;
    }
    int num = 0;
    while(fgets(path, buff.st_size + 1, fptr ) != NULL){
        match = strstr(path, pattern);
            if(match){
            printf("%s %s", file, match);  

        }
    }
    fclose(fptr);
    free(path);    

}

void *worker_thread(void *arg){
    // initialize the ring buffer

    struct worker_args *args = (struct worker_args *)arg;
    struct search_ring_buffer *ring = args->ring;
    const char *pattern = args->pattern;
    struct search_job job;

    while(1){ 
        pthread_mutex_lock(&ring->mutex);
        while (rb_empty(ring)) {
            pthread_cond_wait(&ring->has_job_cond, &ring->mutex);
        }

        // Dequeue a job from the ring buffer
        job = rb_dequeue(ring);

        // Read the file and search for the pattern
        read_file(job.file_path, pattern);
    

        // After processing the job, no need to reassign the file path
        printf("worker threads ready: received job %s",job.file_path );

        // Free the file path memory if necessary, ensure it's done properly
        // Free the file path memory if it was dynamically allocated
        if (job.file_path) {
            free(job.file_path);  // Only free if it was dynamically allocated
        }
        pthread_mutex_unlock(&ring->mutex);
    }
    return NULL;
}


void *search_files(void *arg) {
    FILE *fp;
    
    struct print_job *jobs;

    struct search_ring_buffer *rings;
    struct search_job *_path = (struct search_job *)arg;   
    struct print_queue queue;
    uint64_t match_found = 0;
    struct stat buff;
    char *contents;
    size_t contents_size = buff.st_size;
    int num = 1;

    // Ensure the ring buffer and job are initialized properly
    pthread_mutex_lock(&rings->mutex);
    *_path = rb_dequeue(rings);
    printf("worker threads ready: received job %s",_path->file_path );
    pthread_mutex_unlock(&rings->mutex);

    pq_init(&queue, _path->file_path);
     
    if(lstat(_path->file_path, &buff) < 0){
            err_msg("can't stat");
            return NULL;
        }

    if((fp = fopen(_path->file_path, "r")) == NULL){
            err_msg("can't open");
            return NULL;
        }

    if(!S_ISREG(buff.st_mode)){
        fprintf(stderr, "Error: %s is not a regular file\n", _path->file_path);
        fclose(fp);
        return NULL;
    }

    contents_size = buff.st_size > 0 ? buff.st_size : 1024;       
    contents = malloc(contents_size);
    if (!contents) {
        perror("malloc failed for contents");
        fclose(fp);
        return NULL;
    }
    num = 1;
    while(fgets(contents, contents_size, fp) != NULL){
        char *match_loc = strstr(contents, jobs->match);
        if(match_loc){
           jobs = malloc(sizeof(struct print_job));
           if (!jobs) {
                perror("malloc failed for print_job");
                free(contents);
                fclose(fp);
                return NULL;
            }
            // Fill in the print job details
            jobs->line = strdup(contents);  // Copy the matched line
            if (!jobs->line) {
                perror("Failed to allocate memory for line");
                free(jobs);
                free(contents);
                fclose(fp);
                return NULL;
            }
            char *match =  jobs->match;
            num = jobs->line_num;
            pq_add_tail(&queue, jobs->line, jobs->match, jobs->line_num);

            match_found = 1;

            pq_pop_front(&queue);

            pq_print(&queue, match); 
            break;
            
        }                     
           num++;
                              

        }

        if(match_found){
          pq_print(&queue, jobs->match);
        }else{
        
               printf("No match found in file: %s\n", _path->file_path);
        }
        free(contents);
        fclose(fp);
    

return NULL;

}


void traverse_directory(const char *path) {
   
    struct search_ring_buffer ring;
    struct dirent *dirp;
    DIR *dp;
    int num;
    struct stat statbuf;
    const char *match;
    size_t capacity = MAX_FILES;

    rb_init(&ring, capacity);

    if(lstat(path, &statbuf) < 0){
        perror("cant stat");
        return;
    }
    // Ensure the fullpath buffer is clear before use
    // Not strictly necessary because fullpath is static, but it's safer if you want to reset
    fullpath[0] = '\0';  // Manually clear if needed

    // Construct the full path of the directory
    snprintf(fullpath, sizeof(fullpath), "%s", path);
    if (fullpath[strlen(fullpath) - 1] != '/') {
        snprintf(fullpath + strlen(fullpath), sizeof(fullpath) - strlen(fullpath), "/");
    }
 
    if((dp = opendir(fullpath)) == NULL){
       err_msg("can't open");
    }

    while ((dirp = readdir(dp)) != NULL){  
        printf("files%s\n", dirp->d_name);            
       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;

        // Construct the full path of the file/directory
        snprintf(fullpath + strlen(fullpath), sizeof(fullpath) - strlen(fullpath), "%s", dirp->d_name);

        if (lstat(fullpath, &statbuf) == -1) {
            perror("lstat");
            continue;
        }
        if(S_ISDIR(statbuf.st_mode)){
            traverse_directory(fullpath);
        }else if ( S_ISREG(statbuf.st_mode)){
             // Enqueue the job
            rb_enqueue(&ring, fullpath, statbuf.st_size);
        }
              
    }
    if(closedir(dp) < 0){
        err_quit("can't close %s",dp );
    }
  

}
// Worker thread function prototype
void *worker_thread(void *arg);
void *search_files(void *arg);


int main(int argc, char **argv){
    char *directory_path = ".";
    char *pattern;
    int i, n, err;
    struct search_ring_buffer search_rb;
    struct search_job job;

    if (argc == 2) {
        pattern = argv[1];
        file_print_offset = 2; // Skip "./" prefix if no directory argument

    } else if (argc == 3) {
        pattern = argv[1];
        directory_path = argv[2];
    } else {
        fprintf(stderr,"usage: greptile <pattern> [directory]\n");
        exit(2);
    }

    //uint64_t any_threads_matched = 0;

    rb_init(&search_rb, MAX_FILES);

    pthread_t threads[NUM_THREADS];
   
    struct worker_args worker_args = { .ring = &search_rb, .pattern = pattern };

    for ( i = 0; i < NUM_THREADS; i++){
        pthread_create(&threads[i], NULL, worker_thread, (void *)&worker_args);
    }

    traverse_directory(directory_path);

    // Wait for worker threads to finish (optional - depends on whether you want to join them)
    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

 
    rb_destroy(&search_rb);

    // Return 0 if any thread found a match, 1 otherwise
    //return any_threads_matched == 0;
   return 0;
}

