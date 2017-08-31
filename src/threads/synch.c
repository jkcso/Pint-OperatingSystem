/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Function used to sort condition variables */
static bool condvar_desc_sort (const struct list_elem *a, 
                               const struct list_elem *b, void *aux UNUSED);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_insert_ordered(&sema->waiters,&thread_current()->elem,
                                                  &list_desc_prio_sort,NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   Since the list of waiting threads is not in descending order of
   priority, first we need to sort this list having as goal to bring the
   highest priority thread in front of the list.  If the thread that was
   awaken has higher priority than the one running, then we immediately
   yield to it.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();

  struct thread *t= NULL;
  if (!list_empty (&sema->waiters)) {
    /* if there exist waiting threads, then it sorts them in
       descending order of priority and unblocks the head of the list. */
    list_sort(&sema->waiters,&list_desc_prio_sort,NULL);
    t = list_entry (list_pop_front (&sema->waiters), struct thread, elem);
    thread_unblock (t);
  }
  sema->value++;
  intr_set_level (old_level);

  if (intr_context()) {
  /* If we are indeed running in an interrupt context then we need
     thread_yield() to be called just before the interrupt returns.
     We therefore use the following to cause a new thread to be scheduled. */
    intr_yield_on_return();
  } else {
    if (t != NULL && (t->priority > thread_current()->priority)) {
  /* In the other case we have that the thread waiting has higher priority
     than the currently running thread so the one just unblocked needs to
     yield immediately. */
      thread_yield();
    }
  }
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. 
   
   Method donates the priority to the owner of the acquired lock if the
   current thread has a higher priority */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
 
  // struct semaphore sema = lock->semaphore;
  struct thread *current_thread = thread_current();
  struct thread *lock_owner = lock->holder;

  if (lock_owner != NULL) {
    /* Checks if the current thread has to donate its priority. */
    if (current_thread->priority > lock_owner->priority) {
      /* Donating the thread holding the lock it's priority. */
      lock_owner->priority = current_thread->priority;

      /* For multiple priority donations. */
      list_insert_ordered(&lock_owner->donor_threads,
                  &thread_current()->donor_elem, &list_desc_prio_sort, NULL);
      current_thread->waiting_lock = lock;

      /* Setting nested priority donations */
      struct lock *nested_lock = lock_owner->waiting_lock;
      while (nested_lock != NULL) {
        lock_owner = nested_lock->holder;
        lock_owner->priority = current_thread->priority;
        nested_lock = lock_owner->waiting_lock;
      }
    }
  }

  sema_down(&lock->semaphore);
  current_thread->waiting_lock = NULL;
  lock->holder = thread_current();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  struct semaphore sema = lock->semaphore;
  success = sema_try_down (&sema);

  if (success) {
    // struct thread *current_thread = thread_current();
    lock->holder = thread_current();
  }

  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   If this lock's holder has not been donated anything then the lock is
   released normally. Otherwise we handle priority donations accordingly
   (comments on how this works within the code.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  struct thread *current_thread = thread_current();
  // struct list *donors = &current_thread->donor_threads;
  lock->holder = NULL;
  
  /* If the thread is a donee. */
  if (!list_empty(&current_thread->donor_threads)) {
    struct list_elem *waiting_elem = list_begin(&current_thread->donor_threads);
    struct list_elem *next;

    /* Loop removes donors for lock that are waiting for the lock being 
       released. */
    while (waiting_elem != list_end(&current_thread->donor_threads)) {
      struct thread *waiting_thread = list_entry(waiting_elem, struct thread,
                                                                 donor_elem);
      
      if (waiting_thread->waiting_lock == lock) {
        next = list_remove(waiting_elem);
      } else {
        next = list_next(waiting_elem);
      }

      waiting_elem = next;
    }
    
    /* Sets the current thread to the correct priority. */
    if (!list_empty(&current_thread->donor_threads)) {
      /* Case when thread still has donations, sets the thread to its highest
         donated priority. */
      waiting_elem = list_begin(&current_thread->donor_threads);
      struct thread *waiting_thread = list_entry(waiting_elem, struct thread,
                                                 donor_elem);
      current_thread->priority = waiting_thread->priority;
    /* If no more donations, then set thread back to base priority. */
    } else {
      current_thread->priority = current_thread->base_priority;
    } 
  }

  sema_up(&lock->semaphore); 
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.

   When this method is called the list of waiters is sorted, so
   that when the thread in the waiters list is woken, the thread with
   the highest priority is woken. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  enum intr_level old_level;
	old_level = intr_disable ();

  if (!list_empty (&cond->waiters))
  list_sort(&cond->waiters,&condvar_desc_sort,NULL);
  intr_set_level(old_level);
  sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Sorts the condition variable waiters list in descending order
   of priority. */
static bool condvar_desc_sort (const struct list_elem *a,
                                 const struct list_elem *b, void *aux UNUSED)
{
  struct semaphore_elem *f = list_entry (a, struct semaphore_elem, elem);
  struct list_elem *y = list_front(&f->semaphore.waiters);
  struct thread *t1= list_entry (y, struct thread, elem);

  struct semaphore_elem *e = list_entry (b, struct semaphore_elem, elem);
  struct list_elem *z = list_front(&e->semaphore.waiters);
  struct thread *t2= list_entry (z, struct thread, elem);

  return t1->priority > t2->priority;
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
