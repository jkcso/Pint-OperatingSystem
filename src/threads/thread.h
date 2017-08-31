#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "userprog/syscall.h"
#include "synch.h"
#include "vm/page.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
typedef int pid_t;                      /* Process id. */
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0         /* Lowest priority. */
#define PRI_DEFAULT 31    /* Default priority. */
#define NUM_QUEUES 64     /* Total number of queues in advanced scheduler. */
#define PRI_MAX 63         /* Highest priority. */
#define NICE_MIN -20	    /* Lowest nice. */
#define NICE_DEFAULT 0	     /* Default nice. */
#define NICE_MAX 20          /* Highest nice. */
#define RECENT_CPU_DEFAULT 0 /* Default recent_cpu use */
#define RECENT_CPU_INIT 0    /* Initial value of recent_cpu*/
#define LOAD_AVG_INIT 0	   /* Initial value of load_avg */
#define NO_REMAINDER 0	  /* Used in time slice mod for advanced scheduler. */
#define INCR_VALUE 1	  /* Increment number of ticks in recent_cpu. */
#define SCALE_MUL 100	  /* Multiply recent_cpu and load_avg by 100 times. */
#define SCALE_NICE 2	  /* Factor to multiply niceness. */
#define SCALE_LOAD_AVG 2  /* Factor to multiply load average. */
#define TICKS_TO_UPDATE 4 /* Number of ticks to update. */
#define DIVIDEND 59       /* Number in numerator in load_avg formula. */
#define DIVISOR 60        /* Number in denominator in load_avg formula. */
#define SELF_DIV 1        /* Auxiliary number to aid division. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                        /* Thread identifier. */
    enum thread_status status;        /* Thread state. */
    char name[16];                    /* Thread name. */
    uint8_t *stack;                   /* Saved stack pointer. */
    struct list_elem allelem;         /* List element for all threads list. */

    /* This is the thread's base priority, it does not take into account any
       priority donations */
    int base_priority;

    /* The current effective priority of the thread */
    int priority;

    /* The threads that have donated their priorities to the current thread.
       This list enables us to keep track of multiple donations */
    struct list donor_threads;

    /* list_elem for the list of donor threads */
    struct list_elem donor_elem;

    /* The lock the thread is waiting on (if not then it's null) */
    struct lock *waiting_lock;

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;            /* List element. */

    struct list_elem priority_elem;   /* List element for priority_levels list
                                         (used by advanced scheduler). */

    int8_t nice;                      /* Determines how "nice" the thread
                                         should be to other threads.*/

    int32_t recent_cpu;               /* Measures the amount of CPU time a
                                         thread has received "recently." */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    bool is_user_process;             
    struct thread *parent;            /* The parent of the thread */
    uint32_t *pagedir;                /* Page directory. */
    struct child_data *data;          /* Process data. */
    struct list child_processes;      /* List of child processes. */
    bool filesys_access;              /* Does thread have filesys lock */
    struct list_elem child_elem;      /* list_elem for child processes */
    int next_fd;                      /* Next file descriptor. */
    struct list list_of_fds;          /* List of file descriptors. */
    struct file *executable;          /* Indicates the executable file. */

    /* VM additions */
    struct spt spt;	              /* Suplemental page table */
    void *address_s;                  /* Current stack address */
    struct list mmapped_files;        /* mmapped files */
    mapid_t map_latest_id;            /* smallest unused map_id */
#endif

    /* Owned by thread.c. */
    unsigned magic;                   /* Detects stack overflow. */
  };

/* struct used for files that have been mmapped */
struct map_info{
  mapid_t map_id;                       /* unique mapid_t for mapping */
  void* addr;                           /* addr that mapping was mapped to */
  off_t file_length;                    /* length of mapped file */
  struct file* file;                    /* file that was mapped */
  struct list_elem elem;                /* list_elem for mmapped files list */
};

/* Data about a child process. */
struct child_data {
  pid_t pid;                    /* Child's pid. */
  int return_status;            /* Child's return status. */
  bool loaded_successfully;     /* Indicates id child process loaded
                                  successfully. */
  bool finished;                /* Indicated if child process finished. */
  struct semaphore sync_sema;   /* Used for thread creation synchronisation.*/
  struct semaphore wait_sema;   /* Used for synchronising child's return. */
  struct list_elem elem;        /* Used for instertion in child_process list.*/
};

/* File descriptor holding an open file. */
struct file_descriptor{
  int fd;                              /* Unique file descriptor. */
  struct file *file;                   /* An open file. */
  struct list_elem elem;               /* List elem for list of file
                                          descriptors. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

bool list_desc_prio_sort (const struct list_elem *a,
                                 const struct list_elem *b, void *aux UNUSED);

void sort_ready(void);
struct thread * thread_by_tid(tid_t tid);

struct thread* get_thread(tid_t id);
#endif /* threads/thread.h */
