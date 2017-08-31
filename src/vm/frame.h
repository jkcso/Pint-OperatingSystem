#ifndef FRAME_H_
#define FRAME_H_

#include "lib/kernel/hash.h"
#include "page.h"

/* Represents a frame in our system. */
struct frame {
  void *addr;                 /* Frame address used as a unique field */
  struct page *matching_page; /* Page equivalent to this frame */
  struct thread *owner;       /* The owner thread of both frame & page */
  struct hash_elem ft_elem;   /* Used for insertion in the table */
  struct list_elem lru_elem;  /* Used for insertion in the queue */
};

/* Operations that a frame table should be able to execute to
   effectively capture the concept of virtual memory. */
void ft_init(void);
void ft_destroy(void);
struct frame *ft_add(struct page *page); // TODO struct page
void ft_remove(void *addr);
struct frame *ft_get(void *addr);
void acquire_ft_lock (void);
void release_ft_lock (void);

#endif /* FRAME_H */
