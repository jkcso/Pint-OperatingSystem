#ifndef SWAP_H
#define SWAP_H

#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "page.h"

#define SECTORS_PER_PAGE int_ceiling_div(PGSIZE, BLOCK_SECTOR_SIZE)

// List containing locations to store 
struct list free_page_chunks;

// Hash map containing mappings from page ids to sectors
struct hash used_page_chunks;

// Block used for swap partition
struct block *swap_block;

// Lock used to control swap synchronisation
struct lock *swap_lock;

// Address of swap slot in page sizes
typedef uint32_t page_chunk_addr;

// Stores address of first page-sized chunk never used before.
page_chunk_addr first_unused_chunk;
// Stores swap size in page-sized chunks
page_chunk_addr swap_size_in_chunks;

struct page_chunk {
  page_chunk_addr chunk_address;
  struct list_elem l_elem;
  struct hash_elem h_elem;
};

/* struct swap {
  struct spt used_swaps;
  struct spt free_swaps; // maybe it should be a list
};

// This struct may be needed but could be redundant-We have no clue
struct slot {
  struct page slot_page;
  struct hash_elem elem;
}; */

/* Functions that may be useful for the swap table */
void swap_init(void);
void swap_destroy(void);
size_t swap_write(const void* buff);
void swap_read(size_t key, void *buff);
bool swap_table_full(void);

#endif /* SWAP_H */
