#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H
#include "structs.h"
/**
 * Print the error message 's' and exit(-1)
 */
void terror(char *s);


/**
 * Function to read char by char buffered from a FILE
 */
char buffered_fgetc(char *buffer, uint64_t *pos, uint64_t *read, FILE *f);


void get_num_seqs_and_length(char * seq_buffer, uint64_t * n_seqs, uint64_t * t_len, LoadingDBArgs * ldbargs);

/*
    Generates a queue of tasks for threads
*/
Queue * generate_queue(Head * queue_head, uint64_t t_reads, uint64_t n_threads, uint64_t levels);

/*
    Prints a queue task
*/
void print_queue(Queue * q);

/*
    Gets the next task to do when a pthread is free
*/
Queue * get_task_from_queue(Head * queue_head, pthread_mutex_t * lock);

uint64_t quick_pow4(uint64_t n);

uint64_t quick_pow4byLetter(uint64_t n, const char c);

uint64_t hashOfWord(const unsigned char * word, uint32_t k);

uint64_t asciiToUint64(const char *text);

#endif /* COMMON_FUNCTIONS_H */
