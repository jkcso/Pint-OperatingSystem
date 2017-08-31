#ifndef PAGE_H_
#define PAGE_H_

#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "vm/frame.h"

/* This is the location of the page */
enum page_location {
  MMF,     /* Page in filesys or memory mapped file */
  MEM,     /* Page in memory */
  SWAP     /* Page in swap */
};

#define MAX_STACK 8388608 /* The maximum sixe of the stack is 8MB */
#define NO_OFFSET 0
#define WRITABLE true

struct page {
  void *addr;               /* Address of physical frame */
  struct file file;        /* File that the page may contain */
//  bool read_only;	    /* Indicates if the page is read-only */
//  bool all_zero;          /* Indicates if the page is all zeros */
  uint8_t *upage;           /* from load segment */
  size_t page_read_bytes;   /* from load_segment */
  size_t page_zero_bytes;   /* from load_segment */
  off_t file_offset;        /* File offset */
  bool writable;            /* from load_segment */
//  off_t file_size;        /* Number of bytes of this file */
  enum page_location p_loc; /* Location of the page in memory */
  struct hash_elem elem;    /* Elem for page table entry */
  size_t s_index;           /* Swap table index */
};

/* Supplemental page table */
struct spt {
  struct hash table;	/* Hash table representing the page table */
};

/* Some functions I thought may be useful for implementing this page
   table discussed in the documentation */
void spt_init(struct spt *table);
void spt_destroy(void);
bool spt_page_created(struct file *file,void *addr, off_t file_offset,  
                 bool writable, uint8_t *upage, size_t page_read_bytes, size_t page_zero_bytes);
/*bool spt_page_created(struct file *file,void *page, off_t file_offset,
                        int size, bool read_only, bool all_zero);*/
bool spt_load_page(struct file *file, off_t ofs, bool writable, uint8_t *upage,
                   size_t page_read_bytes, size_t page_zero_bytes);
bool spt_page_removed(struct page *page);
struct page* spt_get(void *addr);
bool new_page_on_stack(void *esp, uint8_t *addr);
bool spt_load(void *address);
struct page* sp_create(uint8_t *page, off_t offset, bool writable, 
                       size_t page_read_bytes, size_t page_zero_bytes);
bool spt_get_frame(struct page *pg, struct frame *frame, void *addr);
/*struct page* sp_create(struct file *file, void *page, off_t offset,
                                            int bytes, bool read_only);*/

#endif /* PAGE_H */
