#include "myQueue.h"
#include "apue.h"
#include <pthread.h>
#include <limits.h>
#include <assert.h>

pthread_mutex_t filelock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  alertWorker = PTHREAD_COND_INITIALIZER;
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
int colorize;

#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"


/* Initialize the print queue with the file path */
void pq_init(struct print_queue *pq, char *file_path) {
    if(!pq || !file_path){
         perror("invalid path"); 
        return;
    }
    //create a copy of the file path string, this is not a concurrent operation
    pq->file_path = file_path;
    if(!pq->file_path){
        perror("strdup() failed for file path");
        return;
    }
    pq->head = NULL;
    pq->tail = NULL;

}

/*add new job at the tail of the queue, if queue empty, add new job at head*/
void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num) {
    struct print_job *job = malloc(sizeof(struct print_job));
    if (!job)
        perror("malloc() failed"); 

    job->line = line;
    if (!job->line) {
        perror("strdup() failed for line");
        free(job);
        exit(EXIT_FAILURE);
    }

    job->match = match;
    if (!job->match) {
        perror("strdup() failed for pattern match");
        free(job->line);
        free(job);
        exit(EXIT_FAILURE);
    }

    job->line_num = line_num;
    job->next = NULL;
   
     // Queue is not empty: append the new job to the tail
    if(pq->head == NULL){
        pq->head = job;
        pq->tail = job;
      
    }else{
        // Queue is empty: set both head and tail to the new job
        pq->tail->next = job;
        pq->tail = job;
    }

}

/* print the job in fifo order*/
struct print_job *pq_pop_front(struct print_queue *pq) {
    if(pq->head == NULL){
        return NULL;
    }
   
    struct print_job *current_job = pq->head; //save the current head
    pq->head = current_job->next; // move the head to the next job

   if(pq->head == NULL){
        pq->tail = NULL; // If the queue is now empty, set tail to NULL
    }
    current_job->next = NULL;  //
    return current_job; // Return the current job to the caller

}

/* Print all jobs in the queue that match the given pattern */
void pq_print(struct print_queue *pq, const char *pattern) {
    if(!pq || !pattern){
        fprintf(stderr, "Invalid queue or pattern.\n");
        return;
    }

    // Start from the head of the queue (no popping)
    struct print_job *current; 
    printf("Jobs matching pattern '%s' in file: %s\n", pattern, pq->file_path);
    while((current = pq_pop_front(pq)) != NULL) {
         int match_offset = current->match - current->line; 
         if(colorize){
         printf(COLOR_GREEN "%d" COLOR_RESET ":%.*s" COLOR_RED "%s" COLOR_RESET "%s\n",
                current->line_num,
                match_offset, current->line,
                pattern,
                current->match);
        } else {
            printf("%d:%s\n", current->line_num, current->line);
        }
            printf("Line: %s\nMatch: %s\nLine Number: %d\n",
                   current->line, current->match, current->line_num);
     // No need to free any jobs here since they remain in the queue
    free(current);
      
    }

   
}