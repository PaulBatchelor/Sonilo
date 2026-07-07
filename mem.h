/* blocklist and blocks */
int blocklist_init(uint32_t *mem, uint16_t list);
uint16_t blocklist_pop(uint32_t *mem, uint16_t list);
int blocklist_push(uint32_t *mem, uint16_t list, uint16_t block);
uint16_t block_to_word(uint16_t blist, uint16_t blk);
uint16_t word_to_block(uint16_t blist, uint16_t wrd);

/* zero page */
uint16_t zero_page_init(uint32_t *mem, uint16_t blist, uint16_t zp);

/* bitset */
void bitset_add(uint32_t *mem, uint16_t bm, uint16_t val);
void bitset_remove(uint32_t *mem, uint16_t bm, uint16_t val);
int bitset_exists(uint32_t *mem, uint16_t bm, uint16_t val);
void bitset_init(uint32_t *mem, uint16_t bm);
int bitset_len(uint32_t *mem, uint16_t bm);
void bitset_free(uint32_t *mem, uint16_t bm, uint16_t blist);

/* array */
void barray_init(uint32_t *mem, uint16_t a);
uint32_t barray_length(uint32_t *mem, uint16_t a);
int barray_append(uint32_t *mem, uint16_t a, uint32_t x);
int barray_pop(uint32_t *mem, uint16_t a, uint32_t *x);
int barray_peak(uint32_t *mem, uint16_t a, uint32_t *x);
int barray_swap(uint32_t *mem, uint16_t a);
int barray_drop(uint32_t *mem, uint16_t a);

/* buddy memory allocator */
void mem_init(uint32_t *mem,
              uint16_t p_top,
              uint16_t p_block,
              uint16_t p_avail,
              uint16_t p_tags);
int mem_alloc(uint32_t *mem, uint16_t p_top, uint16_t k);
uint32_t mem_cksum(uint32_t *mem, uint16_t p_top);
void mem_free(uint32_t *mem, uint16_t p_top, int L, int k);
uint16_t mem_find(uint32_t *mem, uint16_t p_top, uint16_t a);
uint32_t mem_aux(uint32_t *mem, uint16_t pstk);
int mem_klen(uint32_t *mem, uint16_t p_top, int k);
int mem_kavail(uint32_t *mem, uint16_t p_top, int k);

/* global bitmap */
void bits_set(uint32_t *mem, uint32_t off, uint32_t sz, uint32_t w);
uint32_t bits_get(uint32_t *mem, uint32_t off, uint32_t sz);

/* reference counter */
void rc_init(uint32_t *mem, uint16_t r);

/* add: add memory address to the RC list */
int rc_add(uint32_t *mem, uint16_t r, uint16_t m);

/* del: remove memory address from RC list */
int rc_del(uint32_t *mem, uint16_t r, uint16_t m);

/* find: attempts to find entry corresponding with
 * memory address in RC list. Returns slot on succes,
 * and 255 (0xFF) on failure. */
int rc_find(uint32_t *mem, uint16_t r, uint16_t m);

/* sweep: scans through list entries, and marks entries
 * with zero counts as being unused */
int rc_sweep(uint32_t *mem, uint16_t r);

/* get: returns an unused memory block in the RC list */
uint16_t rc_get(uint32_t *mem, uint16_t r);

/* hold: mark a memory address in the RC list to be
 * reserved indefinitely until it is explicitly told not
 * to via "unhold".
 * Note that this address must exist in the RC list already.
 */
int rc_hold(uint32_t *mem, uint16_t r, uint16_t m);
/* unhold: unmarks a memory address in the RC list marked
 * as "reserved". After calling this, the block can be allowed
 * for re-use */
int rc_unhold(uint32_t *mem, uint16_t r, uint16_t m);

/* length: returns number of blocks in RC list */
int rc_length(uint32_t *mem, uint16_t r);

/* incr: increase RC count for memory address */
int rc_incr(uint32_t *mem, uint16_t r, uint16_t m);

/* decr: decrease RC count for memory address */
int rc_decr(uint32_t *mem, uint16_t r, uint16_t m);

/* getters for RC entry fields.
 * count: indicates how many places it is being used.
 * when zero, it will be marked as being available.
 *
 * hold: when set, RC will not free the block.
 */
int rc_get_count(uint32_t *mem, uint16_t r, uint16_t m);
int rc_get_hold(uint32_t *mem, uint16_t r, uint16_t m);

/* nactive: get number of active blocks */
int rc_nactive(uint32_t *mem, uint16_t r);

/* buddy slot allocator */
int allocator_init(uint32_t *mem, uint16_t ctx, uint16_t *out);
int allocator_alloc(uint32_t *mem, uint16_t a, uint8_t sz);
uint16_t allocator_free(uint32_t *mem, uint16_t a, uint16_t m);
int allocator_get_slot(uint32_t *mem, uint16_t ctx, int slot, uint16_t *addr);
int sonilo_alloc(uint32_t *mem, uint16_t ctx, int sz, uint16_t *p);
