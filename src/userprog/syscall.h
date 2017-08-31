#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define ARG_0_OFFSET 1
#define ARG_1_OFFSET 2
#define ARG_2_OFFSET 3
#define BAD_EXIT -1
#define MMAP_ERROR -1
#define READ_FROM_KEYBOARD 0
#define INITIAL_SIZE 0
#define WRITE_TO_CONSOLE 1
#define INITIAL_BUFFER_POS 0
#define MAX_BUFFER 255
#define BYTES_WRITTEN 0

typedef int mapid_t;
struct lock mmap_lock;  // locks access to memory map structures

void exit(int status);
void syscall_init (void);
void acquire_filesys_lock(void);
void release_filesys_lock(void);
void close_all_files(void);
void deny_write_to_exec(char *name_of_file);

#endif /* userprog/syscall.h */
