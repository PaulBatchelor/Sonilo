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
void array_init(uint32_t *mem, uint16_t a);
uint32_t array_length(uint32_t *mem, uint16_t a);
int array_append(uint32_t *mem, uint16_t a, uint32_t x);
int array_pop(uint32_t *mem, uint16_t a, uint32_t *x);
int array_peak(uint32_t *mem, uint16_t a, uint32_t *x);

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
int rc_add(uint32_t *mem, uint16_t r, uint16_t m);
int rc_del(uint32_t *mem, uint16_t r, uint16_t m);
int rc_find(uint32_t *mem, uint16_t r, uint16_t m);
int rc_sweep(uint32_t *mem, uint16_t r);
uint16_t rc_get(uint32_t *mem, uint16_t r);
int rc_hold(uint32_t *mem, uint16_t r, uint16_t m);
int rc_unhold(uint32_t *mem, uint16_t r, uint16_t m);
int rc_length(uint32_t *mem, uint16_t r);
int rc_incr(uint32_t *mem, uint16_t r, uint16_t m);
int rc_decr(uint32_t *mem, uint16_t r, uint16_t m);
int rc_get_count(uint32_t *mem, uint16_t r, uint16_t m);
int rc_get_hold(uint32_t *mem, uint16_t r, uint16_t m);
int rc_get_active(uint32_t *mem, uint16_t r);

/* buddy slot allocator */
int allocator_init(uint32_t *mem, uint16_t ctx, uint16_t *out);
int allocator_alloc(uint32_t *mem, uint16_t a, uint8_t sz);
uint16_t allocator_free(uint32_t *mem, uint16_t a, uint16_t m);
