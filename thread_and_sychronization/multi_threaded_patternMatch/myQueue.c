#include "myQueue.h"
#include "apue.h"
#include <limits.h>
#include <assert.h>
#include <pthread.h>


int colorize;

#define COLOR_RED     "\x1B[91m"
#define COLOR_MAGENTA "\x1B[95m"
#define COLOR_GREEN   "\x1B[92m"
#define COLOR_RESET   "\x1B[0m"

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Initialize the print queue with the file path */
void pq_init(struct print_queue *pq, char *file_path){
    //create a copy of the file path string, this is not a concurrent operation
    pq->file_path = file_path;
    if(!pq->file_path){
        perror("strdup() failed for file path");
        return;
    }
    pq->head = NULL;
    pq->tail = NULL;

}

void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num) {
    pthread_mutex_lock(&queue_mutex);

    printf("Adding job to queue: line %d\n", line_num);

    struct print_job *job = malloc(sizeof(struct print_job));
    if (!job) {
        perror("malloc() failed");
        pthread_mutex_unlock(&queue_mutex);
        return;
    }

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

    pthread_mutex_unlock(&queue_mutex);
}


struct print_job *pq_pop_front(struct print_queue *pq) {
    pthread_mutex_lock(&queue_mutex);

    if (pq->head == NULL) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }

    struct print_job *current_job = pq->head;
    pq->head = current_job->next;

    if (pq->head == NULL) {
        pq->tail = NULL; // If the queue is now empty, reset the tail
    }

    pthread_mutex_unlock(&queue_mutex);
    return current_job;
}
/* Print all jobs in the queue that match the given pattern */
void pq_print(struct print_queue *pq, const char *pattern) {
    if(!pq || !pattern){
        fprintf(stderr, "Invalid queue or pattern.\n");
        return;
    }
    // Start from the head of the queue (no popping)
    struct print_job *current; 
    if (colorize){
        printf(COLOR_MAGENTA "%s\n" COLOR_RESET, pq->file_path);
    }
    else{
        printf("%s\n", pq->file_path);
    }
    printf("Jobs matching pattern '%s' in file: %s\n", pattern, pq->file_path);
    while((current = pq_pop_front(pq)) != NULL) {
         int match_offset = current->match - current->line; 
         if(colorize){
            printf(COLOR_GREEN "%d" COLOR_RESET ":%.*s" COLOR_RED "%s" COLOR_RESET "%s\n",
            current->line_num,
            match_offset, current->line,
            pattern,
            current->match);
        }else{
            printf("%d:%s\n", current->line_num, current->line);
        }   
      
    }

     // No need to free any jobs here since they remain in the queue
    free(current);
}