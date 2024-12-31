#ifndef _MYQUEUH_H
#define _MYQUEUH_H

/*initialize a print job in a single linklist*/
struct print_job {
    char *line;  /*line that pattern match*/
    char *match; // points to the match within line
    int line_num; /*line number of match*/
    struct print_job *next; /*next job in list*/
};

/*initialize a queue of jobs in a doubly linklist*/
struct print_queue {
    char *file_path; /*file path of a single job*/
    struct print_job *head; /*copies of first job in queue*/
    struct print_job *tail; /*copies of last job in queue*/
};


void pq_init(struct print_queue *pq, char *file_path);
void pq_add_tail(struct print_queue *pq, char *line, char *match, int line_num);
struct print_job *pq_pop_front(struct print_queue *pq);
void pq_print(struct print_queue *pq, const char *pattern);





#endif  /* _MYQUEUH_H */