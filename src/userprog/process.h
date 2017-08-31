#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_ARGS_SIZE 4096
#define WORD_SIZE 4
#define STACK_INCR 1
#define STACK_DECR 1
#define PROPER_EXIT 1
#define BAD_EXIT -1
#define INIT_COUNT 0


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
