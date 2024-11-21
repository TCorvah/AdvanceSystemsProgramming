#include "greptile.h"
#include "myQueue.h"
#include <dirent.h>
#include <limits.h>
#include <errno.h>


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

void rb_destroy(struct search_ring_buffer *rb) {
    free(rb->jobs);
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->has_job_cond);
    pthread_cond_destroy(&rb->has_space_cond);
}
bool rb_empty(struct search_ring_buffer *rb){
    return(rb->num_jobs = 0);
}

bool rb_full(struct search_ring_buffer *rb){
    return(rb->capacity >= rb->num_jobs);
}

void rb_enqueue(struct search_ring_buffer *rb, char *file_path, off_t file_size) { 
    struct search_job *job;
    if((job = malloc( sizeof(struct search_job))) == NULL)
       err_sys("malloc failed");
    job->file_path = file_path;
    job->file_size = file_size;
    pthread_mutex_lock(&rb->mutex);
    while(rb_full(rb)){
         pthread_cond_wait(&rb->has_space_cond, &rb->mutex);
    }
    assert(rb->num_jobs < rb->capacity);
    rb->jobs[rb->dequeue_index] = *job;
    rb->num_jobs = regid;
    rb->num_jobs++;
    rb->dequeue_index = (rb->dequeue_index + 1) % rb->capacity;   
    if(rb->enqueue_index == rb->dequeue_index)
       rb->enqueue_index = (rb->dequeue_index + 1) % rb->capacity;
    printf(" new job added to buffer %s\n",file_path );
    pthread_cond_broadcast(&rb->has_job_cond);  
    pthread_mutex_unlock(&rb->mutex);
    pthread_exit(NULL);
    
}

struct search_job rb_dequeue(struct search_ring_buffer *rb) {
    struct search_job job;
    // Implement this function
    pthread_mutex_lock(&rb->mutex);
    rb->num_jobs = regid;
    while(rb->num_jobs <= 0){
        pthread_cond_wait(&rb->has_job_cond, &rb->mutex);
    }
    assert(rb->num_jobs > 0);
    job = rb->jobs[(++rb->enqueue_index)%(rb->capacity)];
    rb->num_jobs--;
    //rb->enqueue_index = (rb->enqueue_index - 1) % rb->capacity;
    pthread_cond_signal(&rb->has_space_cond);
    pthread_mutex_unlock(&rb->mutex);
    rb_destroy(rb);
    return job;
}







void read_file(char *file){
    struct stat buff;
    FILE *fptr;
    char *match, *pattern;
    //open file for readonly

    fptr = fopen(file, "r" );
        if(file == NULL){
           perror("can't open file");
        }
  
    char *path = malloc(buff.st_size);
    int num = 0;
    sprintf(path, " %ld, %ld ", strlen(pattern), strlen(file));
    while(fgets(path, buff.st_size, fptr ) != NULL){
        if((match = strstr(path, pattern))){
            num = sscanf(file, match, pattern);
            printf("%s %d %s %s", file, num, match,pattern );  

        }

    }
    fclose(fptr);
    free(path);

    

}

void *worker_thread(void *arg){
    const char *pattern = arg;
    pthread_detach(pthread_self());
    while(1){
        
        struct search_ring_buffer *ring;
        struct search_job *_pattern = (struct search_job *)pattern;
        pthread_mutex_lock(&ring->mutex);
        *_pattern = rb_dequeue(ring);
        read_file(_pattern->file_path);
         _pattern->file_path = ring->jobs->file_path;
        printf("worker threads ready: received job %s",_pattern->file_path );
        pthread_mutex_unlock(&ring->mutex);
        free(_pattern->file_path);
    }

}


void *search_files(void *arg) {
    //pthread_detach(pthread_self());
    FILE *fp;
    uint64_t match;  
    const char *pattern = ((const char *)arg);
    const char *path = ((const char *)arg);
    char *pat;
    struct print_queue queue;
    struct print_job *jobs;
    struct search_ring_buffer *rings;
    struct search_job *_path = (struct search_job *)path;   
       /*dequeue from ring buffer*/
        pthread_mutex_lock(&rings->mutex);
       *_path = rb_dequeue(rings);
       _path->file_path = rings->jobs->file_path;
       printf("worker threads ready: received job %s",_path->file_path );
       pthread_mutex_unlock(&rings->mutex);

       pq_init(&queue, _path->file_path);
     
       if(lstat(_path->file_path, &buff) < 0){
            err_msg("can't stat");
        }
        if((fp = fopen(_path->file_path, "r")) == NULL)
            err_msg("can't open");
        if(!S_ISREG(buff.st_mode))
            perror("only regular file");

        flockfile(fp); 

        match = 0;
        _path->file_size = buff.st_size;
        char *contents = malloc(buff.st_size);
        int num = 1;
        while(fgets(contents, buff.st_size, fp) != NULL){
            if((pat = strstr(contents, pattern))){
                num++;
                match = 1;
                pq_add_tail(&queue, pat, (char*)pattern, num);            
                pq_pop_front(&queue);
                pq_print(&queue, pattern); 
                break;
            }          

        }
        fclose(fp);
        if(match != 0)
          pq_print(&queue, pattern);
        else
            return NULL;
       
        funlockfile(fp);

return NULL;

}


void traverse_directory(const char *path) {
    // Implement this function 
    struct search_ring_buffer ring;
    struct dirent *dirp;
    DIR *dp;
    char filen[PATH_MAX];
    int num;
    struct stat statbuf;

    if(lstat(filen, &buff) < 0)
        err_sys("can't stat");
    
    if(fullpath[strlen(fullpath)-1] == '\n')
       fullpath[strlen(fullpath) - 1] = 0;
    strcpy(fullpath, path);
    fullpath[strlen(fullpath)- 1] = '/';
 
    if((dp = opendir(fullpath)) == NULL)
       err_msg("can't open");

    while ((dirp = readdir(dp)) != NULL){  
        printf("files%s\n", dirp->d_name);            
       if(strcmp(dirp->d_name, ".") == 0 || 
           strcmp(dirp->d_name, "..") == 0) 
            continue;
       
        while (S_ISREG(statbuf.st_mode))  /* not a directory */
            strcpy(fullpath, dirp->d_name); /* append name after "/" */
            fullpath[strlen(fullpath)- 1] = '/';
    
            printf("path=%s\n", dirp->d_name);

        strcpy(&fullpath[num], dirp->d_name);
        printf("adding job %s to ring buffer", fullpath);
        rb_enqueue(&ring, fullpath, buff.st_size);
              
    }
    if(closedir(dp) < 0)
        err_quit("can't close %s",dp );
   
}

