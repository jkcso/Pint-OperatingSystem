#include "vm/swap.h"
#include <stdbool.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

/* Hash table helper functions */
static unsigned spt_hash(const struct hash_elem *e, void *aux);
static bool spt_less(const struct hash_elem *a, const struct hash_elem *b,
                                                            void *aux);
static void spt_free_entry(struct hash_elem *e, void *aux UNUSED);

/* Initialises the page table */
void spt_init(struct spt *table) {
  hash_init(&(table->table), &spt_hash, &spt_less, NULL);
}

/* Destroys the supplemental page table freeing all resources */
void spt_destroy(void) {
  hash_destroy(&thread_current()->spt.table, &spt_free_entry);
}

bool
spt_load_page(struct file *file, off_t ofs, bool writable, uint8_t *upage,
                   size_t page_read_bytes, size_t page_zero_bytes) {
  bool result = false;
  struct thread *current = thread_current();
  struct spt *spt = &current->spt;
  struct page *page_created = malloc(sizeof(struct page));
  if (page_created != NULL) {
    page_created->file = *file;
    page_created->file_offset = ofs;
    page_created->upage = upage;
    page_created->page_read_bytes = page_read_bytes;
    page_created->page_zero_bytes = page_zero_bytes;
    page_created->writable = writable;
    page_created->p_loc = MMF;
    result = hash_insert(&spt->table, &page_created->elem) == NULL;
  }
  return result;
}

/* Returns true if successfully assigns a page to a file */
bool
spt_page_created(struct file *file,void *addr, off_t file_offset,  
                 bool writable, uint8_t *upage, size_t page_read_bytes, size_t page_zero_bytes)//bool read_only, bool all_zero)
{
  bool result = false;
  struct thread *current = thread_current();
  struct spt *spt = &current->spt;
  struct page *page_created = malloc(sizeof(struct page));
  if (page_created != NULL) {
    page_created->addr = addr;
    page_created->file = *file;
    page_created->file_offset = file_offset;
    page_created->upage = upage;
    page_created->page_read_bytes = page_read_bytes;
    page_created->page_zero_bytes = page_zero_bytes;
    page_created->writable = writable;
    //page_created->file_size = size;
    //page_created->read_only = read_only;
    //page_created->all_zero = all_zero;
    page_created->p_loc = MMF;
    result = hash_insert(&spt->table, &page_created->elem) == NULL;
  }
  return result;
}

/* Returns true if it successfully removes a page from table */
bool
spt_page_removed(struct page *page)
{
  bool result = false;

  /* Check if page passed exists and if matched then remove it */
  struct hash_elem *e = hash_find(&thread_current()->spt.table,&page->elem);
  if (e == NULL) {
    return result;
  }

  hash_delete(&thread_current()->spt.table,&page->elem);
  switch (page->p_loc) {
    case MMF:
      break;
    case SWAP:
      swap_read(page->addr, NULL);
      break;
    case MEM:
      ft_remove(pagedir_get_page(thread_current()->pagedir,page->addr));
      pagedir_clear_page (
        thread_current()->pagedir, page->addr);
      break;
    default: break;
  }

  result = true;
  free(page);
  return result;
}

/* Returns a page of the page table */
struct page* spt_get(void *addr) {
  // Helper I found in vaddr file that round down to nearest page boundary.
  // I thought we may need it.
  void *rounded_addr = pg_round_down(addr);
  struct page to_return;
  to_return.upage = rounded_addr;
  struct hash_elem *e = hash_find(&thread_current()->spt.table,
                                                       &to_return.elem);
  if (e == NULL) {
    return NULL;
  } else {
    return hash_entry(e, struct page, elem);
  }
}

/* Hash function for page table */
static unsigned
spt_hash (const struct hash_elem *e, void *aux UNUSED)
{
  struct page *x = hash_entry(e, struct page,elem);
  return hash_int((int) x->upage);
}

/* Less function for page table */
static bool spt_less (const struct hash_elem *a, const struct hash_elem *b,
                                                      void *aux UNUSED)
{
  struct page *x = hash_entry(a, struct page, elem);
  struct page *y = hash_entry(b, struct page, elem);
  return x->upage < y->upage;
}

/* Frees all the resources associated with the supplemental page table */
static void
spt_free_entry(struct hash_elem *e, void *aux UNUSED)
{
  struct page *page = hash_entry(e,struct page,elem);
  switch (page->p_loc){
  case SWAP:
    swap_read(page->addr, NULL);
    break;
  case MEM:
    ft_remove(pagedir_get_page(thread_current()->pagedir,page->addr));
    pagedir_clear_page (thread_current()->pagedir, page->addr);
    break;
  case MMF:
    break;
  }
  free(page);
}

/* Loads supplemental page table */
/*bool
spt_load(void *address) {
  bool loaded = false;
  void *addr;
  struct spt *spt = &thread_current()->spt;
  struct page *pg;
  struct hash_elem *h_elem;
  / Tries to find an empty page /
  addr = pg_round_down(address);
  pg = spt_get(addr);
  h_elem = hash_find((void *)spt, &pg->elem);

  if (h_elem != NULL) {
    / Tries to load a frame in the empty page found in the hash table /
    struct page *empty_pg = hash_entry(h_elem, struct page, elem);
    struct frame *frame = ft_add(empty_pg);
    //TODO: Fix the read-only stuff...
    if (!install_page(empty_pg->addr, frame->addr, empty_pg->writableempty_pg->read_only)) {
      ft_remove(frame->addr);
      loaded = false;
    }

     Checks if page is indeed empty 
    // Likely unnecessary..
    if (empty_pg->all_zero) {
      memset(frame->addr, NO_OFFSET, PGSIZE);
      empty_pg->all_zero = false;
      loaded = true;
    } else {
      int b_read = 0;
      switch (empty_pg->p_loc) {
        case MMF:
          acquire_filesys_lock();
          b_read = file_read_at(empty_pg->file, frame->addr,
                           file_length(empty_pg->file), empty_pg->file_offset);
          release_filesys_lock();
          if (b_read == file_length(empty_pg->file)) {
            memset(frame->addr + b_read, NO_OFFSET, PGSIZE - b_read);
            loaded = true;
          } else {
            ft_remove(frame->addr);
            pagedir_clear_page(thread_current()->pagedir, empty_pg->addr);
            loaded = false;
          }
          break;
        case MEM:
          loaded = true;
          break;
        case SWAP:
          swap_read_only(empty_pg->s_index, frame->addr);
          loaded = true;
          break;
        default:
          break;
      }
    }
    empty_pg->p_loc = MEM;
  }
  return loaded;
} */

/* Obtains a frame to store the page and fetches the data into the frame */
bool spt_get_frame(struct page *pg, struct frame *frame, void *addr) {
  bool loaded = false;
  frame = ft_add(pg);
  int b_read = 0;

  switch (pg->p_loc) {
    case MMF:
      acquire_filesys_lock();
      b_read = file_read_at(&(pg->file), frame->addr, pg->page_read_bytes, 
                            pg->file_offset);
      release_filesys_lock();
      if (b_read == pg->page_read_bytes) {
        memset(frame->addr + b_read, NO_OFFSET, PGSIZE - b_read);
        loaded = true;
      } else {
        ft_remove(frame->addr);
        pagedir_clear_page(thread_current()->pagedir, pg->upage);
        return false;
      }
    case MEM:
      loaded = true;
      break;
    case SWAP:
      swap_read_only(pg->s_index, frame->addr);
      loaded = true;
      break;
    default:
      break;
  }

  if (!install_page(pg->upage, frame->addr, pg->writable)) {
    ft_remove(frame->addr);
    pagedir_clear_page(thread_current()->pagedir, pg->upage);
    loaded = false;
  }

  pg->p_loc = MEM;
  return loaded;
}

/* Creates a new stack page */
struct page* sp_create(uint8_t *upage, off_t offset,
        bool writable, size_t page_read_bytes, size_t page_zero_bytes)//int bytes, bool read_only)
{
  struct spt *spt = &thread_current()->spt;
  struct page *new_pg = malloc(sizeof(struct page));
  if (new_pg != NULL) {
    //new_pg->read_only = read_only;
    new_pg->upage = upage;
    new_pg->file_offset = offset;
    //new_pg->upage = upage;
    new_pg->page_read_bytes = page_read_bytes;
    new_pg->page_zero_bytes = page_zero_bytes;
    new_pg->writable = writable;
    //new_pg->file_size = bytes;
    new_pg->p_loc = MEM;
    hash_insert(&spt->table, &new_pg->elem);
    return new_pg;
  }
  return NULL;
}

/* Checks if a new page is succesfully put on stack */
bool
new_page_on_stack(void *esp, uint8_t *addr) {
  bool successful = false;
  void *esp_rounded = pg_round_down(esp);
//  void *s_addr = thread_current()->address_s;
  if (esp > PHYS_BASE - MAX_STACK) {
//    esp -= PGSIZE;
    struct page *new_p = sp_create(pg_round_down(addr), NO_OFFSET, WRITABLE, 0,
                                   0);
    if (new_p != NULL) {
      struct frame *new_fr = ft_add(new_p);
      if (new_fr != NULL) {  
        if (install_page(new_p->upage, new_fr->addr,
                         new_p->writable)) {
          successful = true;
        }
      }
    }
  }
  return successful;
}
