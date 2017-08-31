#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/fixed-point.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Arrray of lists of processes, one list for each possible
   priority. The priority of the list represents its index
   in the array (used only in advanced scheduler) */
static struct list priority_levels[NUM_QUEUES];

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* Highest current priority. No thread can have higher priority than this
 at the given moment. Used for efficiency reasons.
 (used only in advanced scheduler) */
static int highest_priority;

/* It estimates the average number of threads ready to run over the past
   minute. Initialised at 0 upon system boot (thread_init) and updated every
   second. Represented as a fixed-point integer. */
static int32_t load_avg;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static void update_priority(struct thread *thread, void *aux UNUSED);
static void update_recent_cpu(struct thread *thread, void *aux UNUSED);
static void update_load_avg(void);
static int highest_non_empty_level(void);
static bool is_mult_of_second(void);
static bool is_mult_of_four(void);
static size_t count_ready_threads(void);

/* Checks whether the system tick counter is at a
   multiple of 4, where priorities should be recalculated. */
static bool
is_mult_of_four(void)
{
  return timer_ticks() % TIME_SLICE == NO_REMAINDER;
}

/* Checks whether the system tick counter has reached
   a multiple of a second, when load_avg and recent_cpu
   must be recalculated using TIMER_FREQ from timer.h */
static bool
is_mult_of_second(void)
{
  return timer_ticks() % TIMER_FREQ == NO_REMAINDER;
}

/* Calculates the total number of ready threads in our system.
   Will be used in the advanced scheduler to share the time fairly
   in a round robin scheme. */
static size_t
count_ready_threads(void)
{
  int ready = list_size(&ready_list);
  if(thread_current() != idle_thread) {
    ready++;
  }
  return ready;
}

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /*	For the advanced scheduler */
  if (thread_mlfqs) {
    /* Initializes each list of the priority_levels */
    for (int i = PRI_MIN; i < NUM_QUEUES; ++i) {
      list_init (&priority_levels[i]);
    }
      /* Initializes load_avg and current_priority to 0 upon system boot. */
      load_avg = LOAD_AVG_INIT;
      highest_priority = PRI_MIN;
  }

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, SEMA_INIT_VALUE);
#ifdef USERPROG
  list_init(&(running_thread()->child_processes));
  running_thread()->data = NULL;
  ft_init();
#endif
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

/* Updates advanced scheduler statistics. */
  if (thread_mlfqs) {
    /* recent_cpu is incremented by 1 for the running thread. */
    if (t != idle_thread) {
      t->recent_cpu = ADD_FIXED_POINT_INT(t->recent_cpu, INCR_VALUE);
    }

    /* Updates for advanced scheduler. */
    if (is_mult_of_second()) {
      /* Update the load_avg and the recent_cpu. */
      update_load_avg();
      thread_foreach (&update_recent_cpu, NULL);
    }

    if (is_mult_of_four()) {
      /* Update priority and the priority list holding current priorities. */
      thread_foreach (&update_priority, NULL);
    }
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{

  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  t->nice = thread_current()->nice;
  t->recent_cpu = thread_current()->recent_cpu;
  tid = t->tid = allocate_tid ();

#ifdef USERPROG
  list_init(&t->child_processes);

  /* VM addition */
  spt_init(&(t->spt));
  list_init(&(t->mmapped_files));
  t->map_latest_id = 0;

  struct child_data *child = malloc(sizeof(struct child_data));
  if (child != NULL) {
    child->pid = t->tid;
    child->return_status = 0;
    child->finished = false;
    child->loaded_successfully = false;
    sema_init(&child->sync_sema,0);
    sema_init(&child->wait_sema,0);
    list_init(&t->list_of_fds);
    t->executable = NULL;
    t->next_fd = 2;
    t->data = child;
   }
#endif

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack'
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);
  if ((t->base_priority > thread_current()->base_priority && thread_mlfqs)
      || (t->priority > thread_current()->priority && !thread_mlfqs)) {
    thread_yield();
  }
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered(&ready_list,&t->elem,&list_desc_prio_sort,NULL);

  /* Manages issues of the advanced scheduler i.e. adds the thread in
     the correct priority_levels queue and updates the highest priority */
  if (thread_mlfqs) {
    list_push_back(&priority_levels[t->priority], &t->priority_elem);
    if (t->priority  > highest_priority) {
       highest_priority = t->priority;
    }
  }

  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();

  if (cur != idle_thread) {
    list_insert_ordered(&ready_list,&cur->elem,&list_desc_prio_sort,NULL);

    /* For the advanced scheduler.  Adds the thread in the correct
       priority_levels queue and updates the highest priority variable.
       Remember that then we use round robin so the order of the list
       is more fair to be first in first served using list push back. */
    if (thread_mlfqs) {
      list_push_back(&priority_levels[cur->priority],&cur->priority_elem);
  	  if (cur->priority > highest_priority) {
  	    highest_priority = cur->priority;
      }
    }
  }

  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {

  /* Disable interrupts to avoid a second thread changing priorities */
  enum intr_level old_intr_level = intr_disable();

  /* Check that new priority is between range 0 to 63 */
  if (new_priority <= 63 && new_priority >= 0) {
    struct thread *current_thread = thread_current();

    /* Checks if the thread is a donee thread, if it is not then it sets the
       current priority to the new priority, if the thread is a donee but its
       current priority is lower than its new priority then it sets its
       current priority to the new base priority and empties the list of
       donations */
    if (list_empty(&current_thread->donor_threads)) {
      current_thread->priority = new_priority;
    } else if (current_thread->priority < new_priority) {
      list_init(&current_thread->donor_threads);
      current_thread->priority = new_priority;
    }

    current_thread->base_priority = new_priority;

    /* Checks to see if the head of the ready queue has a higher priority than
       the current thread, if it does then the current thread is yielded */
    struct list_elem *thread_elem = list_begin(&ready_list);
    struct thread *highest_ready_thread = list_entry(thread_elem, struct
               thread, elem);

    if (highest_ready_thread->priority > current_thread->priority) {
      thread_yield();
    }
  }

  /* Re-enable interrupts once done */
  intr_set_level(old_intr_level);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int new_nice)
{
  ASSERT (new_nice >= (int) NICE_MIN && new_nice <= (int) NICE_MAX);

  struct thread *current = thread_current();
  current->nice = new_nice;

  /* Updating current priority with as per the new nice level */
	if (current != idle_thread) {
		update_priority(current, NULL);
	}

  /* If current thread does not still have the highest priority, yields */
	if (!intr_context() && current->priority < highest_priority) {
	  thread_yield();
	}
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  return FIXED_POINT_TO_INT_TO_NEAREST(SCALE_MUL*load_avg);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  return FIXED_POINT_TO_INT_TO_NEAREST(
                                   SCALE_MUL*(thread_current()->recent_cpu));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  /* Initial thread is also calculating its own priority */
  if (thread_mlfqs) {
    if (t == initial_thread) {
      t->nice = (int8_t)NICE_DEFAULT;
      t->recent_cpu = (int32_t)RECENT_CPU_DEFAULT;
      update_priority(t, NULL);
    } else {
      t->nice = running_thread()->nice;
      t->recent_cpu = running_thread()->recent_cpu;
      update_priority(t, NULL);
    }
  } else {
    t->priority = priority;
  }

  t->base_priority = priority;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  /* Initialise list for priority donations */
  t->base_priority = priority;
  list_init(&t->donor_threads);
  t->waiting_lock = NULL;

  ASSERT(&t->donor_threads != NULL);

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread.

   When choosing a return from the run queue this method should take the head
   of the ready list, this should be the thread with the highest effective
   priority */
static struct thread *
next_thread_to_run (void)
{

  if (list_empty (&ready_list)) {
    return idle_thread;
  }

  if (thread_mlfqs) {
    int highest_non_empty = highest_non_empty_level();
    struct thread *next = list_entry(list_front(
         &priority_levels[highest_non_empty]), struct thread, priority_elem);
    list_remove(&next->priority_elem);
    list_remove(&next->elem);
    return next;
  } else {
    return list_entry(list_pop_front (&ready_list), struct thread, elem);
  }
}

/* Updates the priority of the thread thread, using the
   advanced scheduler priority formula. Keeps the priority
   within limits PRI_MIN and PRI_MAX,
   updates current_priority and priority_levels, keeping track of the
   highest possible priority */
static void
update_priority(struct thread *thread, void *aux UNUSED)
{
  if (thread != idle_thread) {
    int32_t recent_cpu = thread->recent_cpu;
    int8_t nice = thread->nice;
    int old_priority = thread->priority;

    int32_t recent_cpu_part = recent_cpu/TICKS_TO_UPDATE;
    int32_t nice_part = INT_TO_FIXED_POINT(SCALE_NICE*nice, SELF_DIV);
    int32_t fixed_PRIMAX = INT_TO_FIXED_POINT((int32_t)PRI_MAX, SELF_DIV);
    int priority = FIXED_POINT_TO_INT_TO_ZERO(
                                      fixed_PRIMAX-recent_cpu_part-nice_part);

    /* Priority should always be kept within the limits */
    if (priority > (int)PRI_MAX) {
      priority = PRI_MAX;
    } else if (priority < (int)PRI_MIN) {
      priority = PRI_MIN;
    }

    thread->priority = priority;

    /* Inserting the thread in the correct priority level
       in the priority_levels queue if it is ready */
    if (priority != old_priority && thread->status == THREAD_READY) {
      list_remove(&thread->priority_elem);
      list_push_back(&priority_levels[priority], &thread->priority_elem);
    }

    /* Updating current_priority */
    if (highest_priority < priority) {
      highest_priority = priority;
    }
  }
}

/* A function used to update the recent_cpu */
static void
update_recent_cpu(struct thread *thread, void *aux UNUSED)
{
  if (thread != idle_thread) {
    int32_t recent_cpu = thread->recent_cpu;
    int8_t nice = thread->nice;

    /* Computes the coefficient of recent_cpu in advance
       to avoid overflow */
    int32_t twice_load_avg = SCALE_LOAD_AVG*load_avg;
    int32_t recent_cpu_coeff = DIV(twice_load_avg, ADD_FIXED_POINT_INT
                                                  (twice_load_avg, SELF_DIV));
    recent_cpu = ADD_FIXED_POINT_INT(MUL
                                        (recent_cpu_coeff, recent_cpu), nice);
    thread->recent_cpu = recent_cpu;
  }
}

static void
update_load_avg(void)
{
  size_t ready_threads = count_ready_threads();
  int32_t previous_seconds = MUL(INT_TO_FIXED_POINT
                                                (DIVIDEND, DIVISOR),load_avg);
  int32_t new_second = INT_TO_FIXED_POINT(INCR_VALUE, DIVISOR)
                                                              * ready_threads;
  load_avg = previous_seconds + new_second;
}

/* Returns the highest in terms of priority levels priority queue that
   is not empty.  To achieve that it gets the value of the global variable
   holding the highest priority and then and use it as an array index
   in the priority_levels array. */
static int
highest_non_empty_level(void)
{
  int highest_non_empty_level = highest_priority;
  struct list *current_level = &priority_levels[highest_non_empty_level];
  while (list_empty(current_level)) {
    --highest_non_empty_level;
    current_level = &priority_levels[highest_non_empty_level];
  }
  return highest_non_empty_level;
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Sorts the ready list in descending order of priorities */
bool list_desc_prio_sort(const struct list_elem *a, const struct list_elem *b,
                                                             void *aux UNUSED)
{
  struct thread *x = list_entry (a, struct thread, elem);
  struct thread *y = list_entry (b, struct thread, elem);
  if(thread_mlfqs){
    return x->base_priority > y->base_priority;
  } else {
    return x->priority > y->priority;
  }
}

void sort_ready(void)
{
  enum intr_level old_level;
  old_level = intr_disable();
  list_sort(&ready_list,&list_desc_prio_sort,NULL);
  intr_set_level(old_level);
}

struct thread *
get_thread(tid_t id) {
  struct list_elem *elem;
  for (elem = list_begin (&all_list); elem != list_end (&all_list); elem = list_next(elem))
  {
    struct thread *child = list_entry(elem, struct thread, allelem);
    if (child->tid == id)
      return child;
  }
  return NULL;
}

/* Returns the thread with the given thread id */
struct thread *
thread_by_tid(tid_t tid) {

  struct list_elem *e;

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      if (t->tid == tid)
        return t;
    }
  return NULL;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
