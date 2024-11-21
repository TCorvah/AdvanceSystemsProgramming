#include "myQueue.h"
#include "apue.h"
#include <pthread.h>
#include <limits.h>
#include <assert.h>
static char *fullpath;

pthread_mutex_t filelock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  alertWorker = PTHREAD_COND_INITIALIZER;
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

struct print_queue *head = NULL;
struct print_queue *tail = NULL;

void pq_init(struct print_queue *pq, char *file_path) {
    pq->file_path = file_path;
    pq->head = NULL;
    pq->tail = NULL;

}



/*add new job at the tail of the queue, if queue empty, add new job at head*/
void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num) {
    /* malloc the size of jobs*/
    char buff[MAXLINE];
    struct print_job *job = malloc(sizeof(struct print_job));
    if (!job)
        err_sys("malloc() failed"); 

    /* copy the job in queue*/ 
    pthread_rwlock_wrlock(&rwlock);
    strcpy(job->line, line);
    strcpy(job->match, match);
    job->line_num = line_num; 
    /* set variables of job*/
    job->next = NULL;
    job = pq->tail;
    if(pq->tail != NULL)
        pq->tail->next = job;
    else if(pq->head == NULL)
            pq->head = pq->tail = job;
    pq->tail = job;
    pq->tail->line = job->line;
    pq->tail->line_num = job->line_num;
    pq->tail->match = job->match;
    pthread_rwlock_unlock(&rwlock);
    free(job);
    pthread_rwlock_destroy(&rwlock);

}

/* print the job in fifo order*/
struct print_job *pq_pop_front(struct print_queue *pq) {
    pthread_rwlock_wrlock(&rwlock);
    //put this in a conditional lock
    if(pq->head == NULL){
        pthread_rwlock_unlock(&rwlock);
        return NULL;
    }
   
    struct print_job *current_job;
    current_job = pq->head;
    pq->head = current_job->next;
    current_job->next->next = pq->head->next; 
    void *data = current_job;
    free(data);
    pthread_rwlock_unlock(&rwlock); 
    return((struct print_job *)(data));

}

void pq_print(struct print_queue *pq, const char *pattern) {
    //print all items in queue for a job
    pthread_rwlock_wrlock(&rwlock);
    struct print_job job;
    pq->file_path = fullpath;
    assert(strstr(job.match, pattern));
    job.match = (char*)pattern;
    printf("elements found with %s", pattern);
    while(pq->head != NULL){
        pq->head->line = job.line;
        pq->head->line_num = job.line_num;
        pq->head->match = job.match;        
        printf(" pq.filepath = %s\n", pq->file_path);
        printf(" pq.head = %s:%s:%d\n",pq->head->line, pq->head->match, pq->head->line_num);
        pq->head = pq->head->next;
    }
   pthread_rwlock_unlock(&rwlock);

}



