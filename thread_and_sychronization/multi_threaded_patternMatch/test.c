// test_queue.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "myQueue.h"

int main() {
    // 1. Initialize the queue with a file path
    struct print_queue pq;
    pq_init(&pq, "testfile.txt");

    // 2. Add jobs to the queue (tail)
    printf("Adding jobs to the queue...\n");
    pq_add_tail(&pq, "Line 1 content", "pattern1", 1);
    pq_add_tail(&pq, "Line 2 content", "pattern2", 2);
    pq_add_tail(&pq, "Line 3 content", "pattern1", 3);

    // 3. Print all jobs matching pattern 'pattern1'
    printf("\nTesting queue with pattern 'pattern1':\n");
    pq_print(&pq, "pattern1");

    // 4. Print all jobs matching pattern 'pattern2'
    printf("\nTesting queue with pattern 'pattern2':\n");
    pq_print(&pq, "pattern2");

    // 5. Pop a job from the front and print it
    printf("\nPopping jobs from the front:\n");
    struct print_job *popped_job = pq_pop_front(&pq);
    if (popped_job != NULL) {
        printf("Popped job - Line: %s, \nMatch: %s, \nLine Number: %d\n",
               popped_job->line, popped_job->match, popped_job->line_num);
    }

    // 6. Pop another job from the front and print it
    popped_job = pq_pop_front(&pq);
    if (popped_job) {
        printf("Popped job - Line: %s, Match: %s, Line Number: %d\n",
               popped_job->line, popped_job->match, popped_job->line_num);
    
    }

    // 7. Print the remaining jobs in the queue
    printf("\nRemaining jobs in the queue:\n");
    pq_print(&pq, "pattern1");  // You can test for other patterns as well

    // 8. Test the case when the queue is empty
    printf("\nPopping from an empty queue:\n");
    popped_job = pq_pop_front(&pq);
    if (!popped_job) {
        printf("No jobs left to pop.\n");
    }

    return 0;
}
