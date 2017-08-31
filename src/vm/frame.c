#include "frame.h"
#include "swap.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

/* Data structure to hold the frame table */
static struct hash ft;
/* Lock that controls access to the frame table */
static struct lock ft_lock;
/* List that contains a queue of least recently used frames */
static struct list lru_queue;

/* Required for frame table initialisation.   */
static unsigned ft_hash (const struct hash_elem *e, void *aux);

/* Required for frame table initialisation.  */
static bool ft_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Required for frame table eviction. */
static void ft_evict(void);

/* Initialises the frame table */
void
ft_init(void)
{
  /* Initialising the frame hash table */
  hash_init(&ft, &ft_hash, &ft_less, NULL);
  /* Initializing the frame table lock */
  lock_init(&ft_lock);
  /* Initializing the FIFO - first in-fist out queue used for eviction,
     which is done using the Least Recently Used policy */
  list_init(&lru_queue);
}

/* Destroys the frame table and frees resources used */
void
ft_destroy(void)
{
  hash_destroy(&ft, NULL);
}

/* Adds the frame into the frame table and returns the newly created frame */
struct frame *
ft_add(struct page *page)
{
  acquire_ft_lock();
  /* palloc_get_page obtains a single free page and returns its kernel virtual
  address.  If PAL_USER is set then pages are obtained from the user pool,
  otherwise from the kernel pool. */
  void *addr = palloc_get_page(PAL_USER);
  if (addr == NULL) {
    ft_evict();
  	addr = palloc_get_page(PAL_USER);
  }
  struct frame *frame = malloc(sizeof(struct frame));

  /* Checking if malloc return pointer was successful */
  if (frame == NULL) {
    PANIC("Memory allocation failed\n");
  }
  frame->addr = addr;
  frame->matching_page = page;
  frame->owner = thread_current();
  hash_insert(&ft, &frame->ft_elem);
  if(list_empty(&lru_queue)) {
    list_push_front(&lru_queue, &frame->lru_elem);
  } else {
    list_push_back(&lru_queue, &frame->lru_elem);
  }
  release_ft_lock();
  return frame;
}

/* Removing a frame from the frame table and freeing the page related to it */
void
ft_remove(void *addr)
{
  acquire_ft_lock();
  struct frame *rem_frame = ft_get(addr);
  hash_delete(&ft,&rem_frame->ft_elem);
  list_remove(&rem_frame->lru_elem);
  free(rem_frame);
  palloc_free_page(addr);
  release_ft_lock();
}

/* Returns the frame corresponding to the address provided */
struct frame *
ft_get(void *addr)
{
  acquire_ft_lock();
  struct frame frame;
  frame.addr = addr;
  struct hash_elem *elem = hash_find(&ft, &frame.ft_elem);
  struct frame *frame_returned = hash_entry(elem, struct frame, ft_elem);
  release_ft_lock();
  return frame_returned;
}

/* Acquires the frame table lock */
void
acquire_ft_lock (void)
{
  struct thread *current = thread_current();
  if (ft_lock.holder != current) {
    lock_acquire(&ft_lock);
  }
}

/* Releases the frame table lock iff the current thread is its owner */
void
release_ft_lock (void)
{
  struct thread *current = thread_current();
  if (ft_lock.holder == current) {
    lock_release(&ft_lock);
  }
}

/* Hash function for frame table */
static unsigned
ft_hash (const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *frame = hash_entry(e, struct frame, ft_elem);
  return hash_bytes(&frame->matching_page->upage, sizeof(void*));
}

/* Less function for frame table */
static bool
ft_less (const struct hash_elem *a, const struct hash_elem *b,void *aux UNUSED)
{
  struct frame *x = hash_entry(a,struct frame,ft_elem);
  struct frame *y = hash_entry(b,struct frame,ft_elem);
  return x->matching_page->upage < y->matching_page->upage;
}

static void
ft_evict(void)
{
  acquire_ft_lock();
  if (swap_table_full()) {
    release_ft_lock();
    PANIC("Swap table is full");
  }
  //if (list_empty(&lru_queue))
  //  return;
  struct frame *head = list_entry(list_pop_front(&lru_queue),
                                              struct frame, lru_elem);
  head->matching_page->p_loc = SWAP;
  head->matching_page->s_index = swap_write(head->addr);
  pagedir_clear_page(thread_current()->pagedir, head->matching_page->addr);
  ft_remove(head->addr);
  release_ft_lock();
}
