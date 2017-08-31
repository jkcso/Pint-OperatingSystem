#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"
#include "threads/synch.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

/* Syscall function protototypes. */
void halt(void);
void exit(int status);
pid_t exec(const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* Used to validate pointers for correct memory access */
bool is_correct_user_memory(void *address);
static struct file_descriptor *lookup_fd_struct(int fd);

// Lock used to ensure only one thread at a time has filesys access
static struct lock filesys_access;

/* Methods to acquire and release filesys_access */
void acquire_filesys_lock(void);
void release_filesys_lock(void);

void close_all_files(void);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_access);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  // Firstly we would need to retrieve the stack pointer.
  uint32_t *esp_ptr = (uint32_t*) f->esp;

  /* Check whether value of stack pointer is valid */
  if (!is_correct_user_memory(esp_ptr)) {
    exit(BAD_EXIT);
  }

  // Finally using a function number to go down the switch statement.
  uint32_t function_no = *esp_ptr;

  // Then pointers to arguments using pointer arithmetic.
  uint32_t *arg0_ptr = esp_ptr + ARG_0_OFFSET;
  uint32_t *arg1_ptr = esp_ptr + ARG_1_OFFSET;
  uint32_t *arg2_ptr = esp_ptr + ARG_2_OFFSET;

  switch (function_no) {

  case SYS_HALT:
  {
    halt();
    break;
  }

  case SYS_EXIT:
  {
    if (is_correct_user_memory(arg0_ptr)) {
      int status = (int)(*arg0_ptr);
      exit(status);
    }
      break;
  }

  case SYS_EXEC:
  {
    if (is_correct_user_memory(arg0_ptr)) {
      char *cmd_line = (char*)(*arg0_ptr);
      if(cmd_line == NULL || !is_correct_user_memory(cmd_line)
                      || !is_user_vaddr((void*)cmd_line)) {
        exit(BAD_EXIT);
      }
      f->eax = exec(cmd_line);
    }
    break;
  }
 
  case SYS_WAIT:
  {
    if (is_correct_user_memory(arg0_ptr)) {
      pid_t pid = (pid_t)(*arg0_ptr);
      f->eax = wait(pid);
    }
      break;
  }

  case SYS_CREATE:
  {
    if (is_correct_user_memory(arg0_ptr) && is_correct_user_memory(arg1_ptr)) {
      char *file = (char*)(*arg0_ptr);
      if(is_correct_user_memory(file)){
        unsigned initial_size =  (unsigned)(*arg1_ptr);
        if (file == NULL || !is_user_vaddr((void*)file)) {
	  exit(BAD_EXIT);
	}
	  f->eax = create(file, initial_size);
      }
    }
    break;
  }

  case SYS_REMOVE:
  {
    if (is_correct_user_memory(arg0_ptr)) {
      char *file = (char*)(*arg0_ptr);
      if (file == NULL || !is_user_vaddr((void*)file)) {
        exit(BAD_EXIT);
      }
      f->eax = remove(file);
    }
      break;
   }

  case SYS_OPEN:
  {
    if (is_correct_user_memory(arg0_ptr)) {
      char *file = (char*)(*arg0_ptr);
      if (file == NULL || !is_correct_user_memory(file)
                            || !is_user_vaddr((void*)file)){
      exit(BAD_EXIT);
      }
      f->eax = open(file);
    }
    break;
  }

  case SYS_FILESIZE:
  {
    if(is_correct_user_memory(arg0_ptr)) {
      int fd = (int)(*arg0_ptr);
      f->eax = filesize(fd);
    }
    break;
  }

  case SYS_READ:
  {
  if (is_correct_user_memory(arg0_ptr) && is_correct_user_memory(arg1_ptr) &&
                                          is_correct_user_memory(arg2_ptr)) {
    int fd = (int)(*arg0_ptr);
    void *buffer = (void*)(*arg1_ptr);
    unsigned size = (unsigned)(*arg2_ptr);
    if (buffer == NULL || !is_user_vaddr(buffer)) {
      exit(BAD_EXIT);
    }
      f->eax = read(fd, buffer, size);
    }
    break;
  }

  case SYS_WRITE:
  {
  if(is_correct_user_memory(arg0_ptr) && is_correct_user_memory(arg1_ptr) &&
                                         is_correct_user_memory(arg2_ptr)) {
    int fd = (int)(*arg0_ptr);
    void* buffer =  (void*)(*arg1_ptr);
    unsigned size = *arg2_ptr;
    if(buffer == NULL || !is_correct_user_memory(buffer)
                                 || !is_user_vaddr(buffer)) {
      exit(BAD_EXIT);
    }
      f->eax = write(fd, buffer, size);
    }
      break;
   }

  case SYS_SEEK:
  {
    if(is_correct_user_memory(arg0_ptr) && is_correct_user_memory(arg1_ptr)) {
      int fd = (int)(*arg0_ptr);
      unsigned position = (unsigned)(*arg1_ptr);
      seek(fd, position);
    }
    break;
  }

  case SYS_TELL:
  {
    if(is_correct_user_memory(arg0_ptr)) {
      int fd = (int)(*arg0_ptr);
      f->eax = tell(fd);
    }
    break;
  }

  case SYS_CLOSE:
  {
    if(is_correct_user_memory(arg0_ptr)) {
      int fd = (int)(*arg0_ptr);
      close(fd);
    }
    break;
  }

  case SYS_MMAP:
  {
    if(is_correct_user_memory(arg0_ptr)) {
      int fd = (int)(*arg0_ptr);
      void *addr = (void*)(*arg1_ptr);
      f->eax = mmap(fd,addr);
    }
    break;
  }

  case SYS_MUNMAP:
  {
    if(is_correct_user_memory(arg0_ptr)) {
      mapid_t mapping = (mapid_t)(*arg0_ptr);
      munmap(mapping);
    }
    break;
  }

  break;
  }
}

/* System call functions implementation */

/* Terminates Pintos by calling shutdown_power_off() (declared in
‘devices/shutdown.h’). This should be seldom used, because you lose
some information about possible deadlock situations, etc. *Warning*:
The original Pintos documentation on the Stanford website is outdated
and incorrectly places the shutdown function in the wrong location.
It’s advisable that you don’t use it as a reference in completing any
of the tasks.*/

void halt(void) {
  shutdown_power_off();
}

/* Terminates the current user program, sending its exit status to the kernel.
 If the process’s parent waits for it (see below), this is the status that
 will be returned. Conventionally, a status of 0 indicates success and nonzero
 values indicate errors. */

void exit(int status) {
  thread_current()->data->return_status = status;
  thread_exit();
}

/* Runs the executable whose name is given in cmd line, passing any given
arguments, and returns the new process’s program id (pid). Must return pid -1,
which otherwise should not be a valid pid, if the program cannot load or run
for any reason. Thus, the parent process cannot return from the exec until it
knows whether the child process successfully loaded its executable. You must
use appropriate synchronization to ensure this. */
pid_t exec(const char *cmd_line) {
  char *name_of_file;
  char *save;

  pid_t process_id = process_execute(cmd_line);

  if(process_id == TID_ERROR){
    return BAD_EXIT;
  }

  return process_id;
}

/* Waits for a child process pid and retrieves the child’s exit status.
wait must fail and return -1 immediately if any of the following conditions
is true:

• pid does not refer to a direct child of the calling process. pid is a direct
 child of the calling process if and only if the calling process received pid
 as a return value from a successful call to exec.
Note that children are not inherited: if A spawns child B and B spawns child
process C, then A cannot wait for C, even if B is dead. A call to wait(C) by
process A must fail. Similarly, orphaned processes are not assigned to a new
parent if their parent process exits before they do.

• The process that calls wait has already called wait on pid. That is, a
process may wait for any given child at most once. */
int wait (pid_t pid) {
  return process_wait(pid);
}

/* Creates a new file called file initially initial size bytes in size.
Returns true if successful, false otherwise. Creating a new file does not open
 it: opening the new file is a separate operation which would require a open
 system call. */
bool create (const char *file, unsigned initial_size) {
 bool file_created;
 struct thread *current_thread = thread_current();

 // Prevent access to file system when creating new file
 lock_acquire(&filesys_access);
 current_thread->filesys_access = true;
 file_created = filesys_create(file, initial_size);
 // Allow other threads access to file system once file system used
 lock_release(&filesys_access);
 current_thread->filesys_access = false;
 return file_created;
}

/* Deletes the file called file. Returns true if successful, false otherwise.
A file may be removed regardless of whether it is open or closed, and removing
an open file does not close it. */
bool remove (const char *file) {
  bool file_deleted;
  struct thread *current_thread = thread_current();

  // Prevent access to file system when deleting file
  lock_acquire(&filesys_access);
  current_thread->filesys_access = true;
  file_deleted = filesys_remove(file);
  // Allow other threads access to file system once file deleted
  lock_release(&filesys_access);
  current_thread->filesys_access = false;
  return file_deleted;
}

/* Opens the file called file. Returns a nonnegative integer handle called a
“file descriptor” (fd), or -1 if the file could not be opened. */
int open (const char *file) {
  /* Opens the file using the file systems. */
  acquire_filesys_lock();
  struct file *file_struct = filesys_open(file);  
//  file_deny_write(file_struct);
  release_filesys_lock();

  /* Checks is such a file exists, otherwise returns -1. */
  if (file_struct != NULL) {
    /* Assignes a file discriptor to the file. */
    struct file_descriptor *file_descriptor = malloc(sizeof(struct file_descriptor));
    struct thread *current = thread_current();
    int fd = current->next_fd;
    ++(current->next_fd);
    file_descriptor->fd = fd;
    file_descriptor->file= file_struct;
    /* Adds the file discriptor in the list of the process file disrciptors. */
    list_push_back(&current->list_of_fds, &file_descriptor->elem);
    return fd;
  } else {
    return BAD_EXIT;
  }
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize (int fd) {
  struct file_descriptor *current_fd = lookup_fd_struct(fd);
  if (current_fd != NULL) {
    struct file *current_file = current_fd->file;
    acquire_filesys_lock();
    off_t size = file_length(current_file);
    release_filesys_lock();
    return size;
  }
  return BAD_EXIT;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number
of bytes actually read (0 at end of file), or -1 if the file could not be read
(due to a condition other than end of file). Fd 0 reads from the keyboard
using input_getc(), which can be found in ‘src/devices/input.h’. */
int read (int fd, void *buffer, unsigned size) {
  /* File descriptor reads form keyboard. */
  if (fd == READ_FROM_KEYBOARD) {
    uint8_t in_size = INITIAL_SIZE;
    while (in_size <= size) {
      in_size += input_getc();
    }
    return in_size;
  }
  /* Reads from file. */
  struct file_descriptor *current_fd = lookup_fd_struct(fd);
  if (current_fd != NULL) {
    struct file *current_file = current_fd->file;
    acquire_filesys_lock();
    off_t read = file_read(current_file, buffer, size);
    release_filesys_lock();
    return read;
  } else {
    return BAD_EXIT;
  }
}

/* Writes size bytes from buffer to the open file fd. Returns the number of
bytes actually written, which may be less than size if some bytes could not
be written.
Writing past end-of-file would normally extend the file, but file growth is
not implemented by the basic file system. The expected behavior is to write
as many bytes as possible up to end-of-file and return the actual number
written, or 0 if no bytes could be written at all. */
int write (int fd, const void *buffer, unsigned size) {
  /* Writes to the console. */
  if (fd == WRITE_TO_CONSOLE) {
    unsigned buffer_pos = INITIAL_BUFFER_POS;
    unsigned size_count = size;
    if (size > MAX_BUFFER) {
      while (size_count > MAX_BUFFER) {
        putbuf(buffer + buffer_pos, MAX_BUFFER);
        size_count -= MAX_BUFFER;
        buffer_pos += MAX_BUFFER;
      }
    }
    putbuf(buffer + buffer_pos, size_count);
    return size;
  }

  /* Writes to the file. */
  struct file_descriptor *current_fd = lookup_fd_struct(fd);
  if (current_fd != NULL) {
    struct file *current_file = current_fd -> file;
    acquire_filesys_lock();
    off_t write = file_write(current_file, buffer, size);
    release_filesys_lock();
    return write;
  }
  /* Returns no bytes are written. */
  return BYTES_WRITTEN;
}


/* Changes the next byte to be read or written in open file fd to position,
expressed in bytes from the beginning of the file. (Thus, a position of 0 is
the file’s start.) */
void seek (int fd, unsigned position) {
  struct file_descriptor *current_fd = lookup_fd_struct(fd);
  if (current_fd != NULL) {
    struct file *current_file = current_fd->file;
    acquire_filesys_lock();
    file_seek(current_file, position);
    release_filesys_lock();
  }
}

/* Returns the position of the next byte to be read or written in open file
fd, expressed in bytes from the beginning of the file. */
unsigned tell (int fd) {
  struct file_descriptor *current_fd = lookup_fd_struct(fd);
  if (current_fd != NULL) {
    struct file *current_file = current_fd->file;
    acquire_filesys_lock();
    off_t pos = file_tell(current_file);
    release_filesys_lock();
    return pos;
  }
  return 0;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
closes all its open file descriptors, as if by calling this function for
each one. */
void close (int fd) {
  /* Gets the file informations from the list of the process fds. */
  struct file_descriptor *file_descriptor = lookup_fd_struct(fd);

  /* Checks if the fd found corresponds to a file.
     If it does so, then close the file,
     remove its descriptor from the list and free the memory.*/
  if (file_descriptor != NULL) {
    acquire_filesys_lock();
    file_close(file_descriptor->file);
    release_filesys_lock();
    list_remove(&file_descriptor->elem);
    free(file_descriptor);
  }
}

/* Method maps the file open as an fd into the process's virtual address space.
The entire file is mapped into consecutive virtual pages starting at addr. */
mapid_t mmap(int fd, void *addr) {
  int num = (((int)addr) - (int) PHYS_BASE) % PGSIZE;
  if (addr == 0 || fd == 0 || fd == 1 || 
      (((int)addr) - (int) PHYS_BASE) % PGSIZE != 0) {
    return MMAP_ERROR;
  }
  
  /* Gets the file informations from the list of the process fds. */
  struct file_descriptor *file_descriptor = lookup_fd_struct(fd);

  struct file* mfile = file_descriptor->file;
  acquire_filesys_lock();
  mfile = file_reopen(mfile);
  off_t file_len = file_length(mfile);
  release_filesys_lock();

  if (file_len == 0 || file_descriptor->file == NULL) {
    return MMAP_ERROR;
  }

  int file_pg_size = file_len / PGSIZE;

  if (file_len % PGSIZE != 0) {
    file_pg_size++;
  }

  void* upage = addr;
  off_t file_offset = 0;
  void* upper_bound_addr = addr + (file_pg_size * PGSIZE);
  for(upage; upage <= upper_bound_addr; upage += PGSIZE){
    struct page* pg = spt_get(upage);
    if(pg != NULL){
      return MMAP_ERROR;
    }
    size_t page_read_bytes = PGSIZE;
    if(upage + PGSIZE > upper_bound_addr)
      page_read_bytes = file_len - file_offset;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    if(!spt_load_page(mfile, file_offset, true, upage, 
                                page_read_bytes, page_zero_bytes))
      return MMAP_ERROR;    
   } 
/*
  addr_offset_ptr = addr;
  void* file_offset_ptr = 0;
  for(addr_offset_ptr; addr_offset_ptr < addr + (file_pg_size * PGSIZE);
      addr_offset_ptr += PGSIZE){
        //struct page* pg = 
//    struct page* pg = spt_get(addr_offset_ptr);
//TODO: Fix page creation here!
    bool success = true;/*spt_page_created(mfile, addr_offset_ptr, file_offset_ptr,
                                    file_len, false, false);*/
  /*  if (!success) {
      return MMAP_ERROR;
    }

    file_offset_ptr += PGSIZE;
 }*/

  lock_acquire(&mmap_lock);
  struct map_info* map;
  map->map_id = thread_current()->map_latest_id;
  map->file_length = file_len;
  map->addr = addr;
  map->file = mfile;
  struct list mmap_files = thread_current()->mmapped_files;
  
  list_insert(list_tail(&mmap_files), &(map->elem));
  lock_release(&mmap_lock);
  return (thread_current()->map_latest_id)++;
}

/* Unmaps the mapping designated by mapping, which must be a mapping id 
returned by a previous call to mmap by the same process that has not yet been
unmapped */
void munmap(mapid_t mapping) {
// TODO
  lock_acquire(&mmap_lock);
  struct map_info* map = NULL;
  struct list_elem* e;
  struct list mmap_files = thread_current()->mmapped_files;
  for(e = list_begin(&mmap_files); e != list_end(&mmap_files); 
      e = list_next(e)){
    struct map_info* m = list_entry(e, struct map_info, elem);
    if(m->map_id == mapping){
      map = m;
      break;
    }
  }

  if(map == NULL){
    lock_release(&mmap_lock);
    return MMAP_ERROR;
  }

  /* Gets the file informations from the list of the process fds. */
  struct file *file = lookup_fd_struct(map->file);
  off_t file_len = map->file_length;

  if (file_len == 0 || file == NULL) {
    lock_release(&mmap_lock);
    return MMAP_ERROR;
  }

  int file_pg_size = file_len / PGSIZE;

  if (file_len % PGSIZE != 0) {
    file_pg_size++;
  }

  void* addr = map->addr;
  void* addr_offset_ptr = addr;
  for(addr_offset_ptr; addr_offset_ptr <= addr+file_pg_size;
      addr_offset_ptr += PGSIZE){
    struct page* pg = spt_get(addr_offset_ptr);
    bool success = spt_page_removed(pg);
    if(!success){
      lock_release(&mmap_lock);
      return MMAP_ERROR;
    }
  }

  list_remove(&(map->elem));
  lock_release(&mmap_lock);
}

/* This method checks for safe memory access. Dereferencing occurs in caller.*/
bool is_correct_user_memory(void *address) {
  struct thread *current = thread_current();
  uint32_t *pagedir = current->pagedir;
  bool is_valid = address != NULL &&
  	is_user_vaddr(address)&& (pagedir_get_page(pagedir, address) != NULL);

  if(!is_valid) {
  	exit(BAD_EXIT);
  }
  return is_valid;
}

/* Acquires the filesys lock iff the current thread is not its owner */
void
acquire_filesys_lock (void)
{
  struct thread *current = thread_current();
  if (filesys_access.holder != current) {
    lock_acquire(&filesys_access);
  }
}

/* Releases the filesys lock iff the current thread is its owner */
void
release_filesys_lock (void)
{
  struct thread *current = thread_current();
  if (filesys_access.holder == current) {
    lock_release(&filesys_access);
  }
}

/* Looks over the descriptor table and returns the file descriptor associated
   with the given fd */
static struct file_descriptor *
lookup_fd_struct(int fd)
{
  struct thread *t = thread_current();
  for (struct list_elem *e =
    list_begin (&t->list_of_fds); e != list_end (&t->list_of_fds);
      e = list_next (e)) {
        struct file_descriptor *file_descriptor =
          list_entry (e, struct file_descriptor, elem);
        if (file_descriptor->fd == fd)
         return file_descriptor;
  }
  return NULL;
}

/*Iterates through the descriptor table and closes all the executable files */
void
close_all_files (void)
{
  acquire_filesys_lock();
  struct thread *t = thread_current();
  struct list_elem *e;
  for (e = list_begin (&t->list_of_fds); e != list_end (&t->list_of_fds); )
    {
       struct file_descriptor *file_descriptor = list_entry (e, struct file_descriptor, elem);
       file_close(file_descriptor->file);
       e = list_next (e);
       free(file_descriptor);
    }
  if (thread_current()->parent == NULL) {
    file_close(thread_current()->executable);
  } else {
    file_deny_write(thread_current()->executable);
  }
  release_filesys_lock();
}

void
deny_write_to_exec(char *name_of_file) {
  acquire_filesys_lock();
  struct file *file_struct = filesys_open(name_of_file);
  if (file_struct != NULL) {
    thread_current()->executable = file_struct;
    file_deny_write(file_struct);
  }
  release_filesys_lock();
}
