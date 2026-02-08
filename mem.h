void blocklist_init(uint32_t *mem, uint16_t list);
uint16_t blocklist_pop(uint32_t *mem, uint16_t list);
int blocklist_push(uint32_t *mem, uint16_t list, uint16_t block);
void bitset_add(uint32_t *mem, uint16_t bm, uint16_t val);
void bitset_remove(uint32_t *mem, uint16_t bm, uint16_t val);
int bitset_exists(uint32_t *mem, uint16_t bm, uint16_t val);
void bitset_init(uint32_t *mem, uint16_t bm);
int bitset_len(uint32_t *mem, uint16_t bm);
void mem_init(uint32_t *mem,
              uint16_t p_top,
              uint16_t p_block,
              uint16_t p_avail,
              uint16_t p_tags);
int mem_alloc(uint32_t *mem, uint16_t p_top, uint16_t k);
uint32_t mem_cksum(uint32_t *mem, uint16_t p_top);
void mem_free(uint32_t *mem, uint16_t p_top, int L, int k);
int mem_array_init(uint32_t *mem, uint16_t a);
int mem_array_append(uint32_t *mem, uint16_t a, uint32_t x);
int mem_array_pop(uint32_t *mem, uint16_t a, uint32_t *x);
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
