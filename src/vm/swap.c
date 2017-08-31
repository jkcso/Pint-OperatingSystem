#include "swap.h"

static unsigned swap_hash(const struct hash_elem *e, void *aux);
static bool swap_less (const struct hash_elem *a, const struct hash_elem *b,
                                                             void *aux);

int int_ceiling_div(int a, int b){
  if(a % b == 0){
    return a/b;
  }
  return a/b + 1;
}

/* Initialises the swap partition */
void swap_init() {
  struct block *swap_block = block_get_role(BLOCK_SWAP);
  lock_init(swap_lock);
  page_chunk_addr first_unused_chunk = 0;
  uint32_t swap_size_in_chunks = block_size(swap_block) / SECTORS_PER_PAGE;
  hash_init(&used_page_chunks, &swap_hash, &swap_less, NULL);
  list_init(&free_page_chunks);
}

/* Destroys the swap table freeing all resources */
void swap_destroy() {
  acquire_swap_lock();
  hash_destroy(&used_page_chunks, NULL);
  release_swap_lock();
}

/* Method that gets a free page and puts the given page in the free_swaps */
size_t swap_write(const void *buff) {
  acquire_swap_lock();
  page_chunk_addr chunk_addr = 0;
  
  if(!list_empty(&free_page_chunks)) {
    struct list_elem *elem = list_begin(&free_page_chunks);
    struct page_chunk *pg_chunk = list_entry(elem, struct page_chunk, l_elem);
    chunk_addr = pg_chunk->chunk_address;
    void* buff_offset_ptr = buff;
    write_to_block(chunk_addr);
    list_remove(elem);
    hash_insert(&used_page_chunks, &(pg_chunk->h_elem));
  } else {
    write_to_block(first_unused_chunk);
    struct page_chunk *pg_chunk;
    pg_chunk->chunk_address = first_unused_chunk;
    chunk_addr = first_unused_chunk;
    hash_insert(&used_page_chunks, &(pg_chunk->h_elem));
    first_unused_chunk++;
  }

  release_swap_lock();
  return chunk_addr;
}

/* Method that reads swap from table and frees this swap slot */
void swap_read(size_t key, void *buff) {
  acquire_swap_lock();
  struct page_chunk *page_chunk_lookup;
  page_chunk_lookup->chunk_address = key;
  struct hash_elem *page_chunk_elem = hash_delete(&used_page_chunks, 
                                         &(page_chunk_lookup->h_elem));
  
  if (page_chunk_elem != NULL) {
    struct page_chunk *pg_chunk = hash_entry(page_chunk_elem, struct page_chunk,
                                            h_elem);
    int i;
    void* buff_offset_ptr = buff;
    page_chunk_addr addr = pg_chunk->chunk_address;
    
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
      block_read(swap_block, (addr * SECTORS_PER_PAGE) + i, buff_offset_ptr);
      buff_offset_ptr += BLOCK_SECTOR_SIZE;
    }
   
    list_insert(list_tail(&free_page_chunks), &(pg_chunk->l_elem));
  }

  release_swap_lock();
}

/* Method that reads swap from table and frees this swap slot */
void swap_read_only(size_t key, void *buff) {
  acquire_swap_lock();
  struct page_chunk *page_chunk_lookup;
  page_chunk_lookup->chunk_address = key;
  struct hash_elem *page_chunk_elem = hash_find(&used_page_chunks, 
                                         &(page_chunk_lookup->h_elem));
  
  if (page_chunk_elem != NULL) {
    struct page_chunk *pg_chunk = hash_entry(page_chunk_elem, struct page_chunk,
                                            h_elem);
    int i;
    void* buff_offset_ptr = buff;
    page_chunk_addr addr = pg_chunk->chunk_address;
    
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
      block_read(swap_block, (addr * SECTORS_PER_PAGE) + i, buff_offset_ptr);
      buff_offset_ptr += BLOCK_SECTOR_SIZE;
    }
  }

  release_swap_lock();
}

/* Method that checks if the swap table is full */
bool swap_table_full(void) {
  if(list_empty(&free_page_chunks) && 
     first_unused_chunk >= swap_size_in_chunks) {
    return true;
  }

  return false;
}

/* Acquires the frame table lock */
void
acquire_swap_lock (void)
{
  struct thread *current = thread_current();
  if (swap_lock->holder != current) {
    lock_acquire(swap_lock);
  }
}

/* Releases the frame table lock iff the current thread is its owner */
void
release_swap_lock (void)
{
  struct thread *current = thread_current();
  if (swap_lock->holder == current) {
    lock_release(&swap_lock);
  }
}

/* Hash function for used_chunks_table */
static unsigned
swap_hash(const struct hash_elem *e, void *aux) {
  struct page_chunk *pg_chunk = hash_entry(e, struct page_chunk, h_elem);
  return hash_int(pg_chunk->chunk_address);
}

/* Less function for used_chunks_table */
static bool swap_less (const struct hash_elem *a, const struct hash_elem *b,
                                                             void *aux) {
  struct page_chunk *x = hash_entry(a, struct page_chunk, h_elem);
  struct page_chunk *y = hash_entry(b, struct page_chunk, h_elem);
  return x->chunk_address < y->chunk_address;
}


/* Method writes a page to a block */
void
write_to_block(page_chunk_addr addr, const void *buff) {
  int i;
  void* buff_offset_ptr = buff;
  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_write(swap_block, (addr * SECTORS_PER_PAGE) + i, buff_offset_ptr);
    buff_offset_ptr += BLOCK_SECTOR_SIZE;
  }
}
