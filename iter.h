enum {
    ITER_NONE = 0,
    ITER_ARRAY = 1,
    ITER_LOOKUP = 2
};

enum {
    ITER_ARRAY_LOOP = 0
};

/* alloc: allocate memory for an initializer */
int iter_alloc(uint32_t *mem, uint16_t ctx, uint16_t *i);

/* init: create an empty iterator. it does nothing */
int iter_init(uint32_t *mem, uint16_t p);

/* array: set up an initialized iterator to point to an array */
int iter_array(uint32_t *mem, uint16_t i, uint16_t a);

/* next: get the next value, returned as a slice
 * dereference it with array_value() */
uint32_t iter_next(uint32_t *mem, uint16_t i);

/* iter_real: next, but resolve to a real (float) value */
float iter_real(uint32_t *mem, uint16_t i);

/* iter_get: get current value of iterator */
uint32_t iter_get(uint32_t *mem, uint16_t i);

/* iterator block */

/* new: allocates and initializes an iterator block */
int iter_block_new(uint32_t *mem, uint16_t ctx, uint16_t *ib);

/* compute: fill iterator block (ib) using iterator (it) and 
 * input signal (in) containing trigger */
int iter_block_compute(uint32_t *mem,
        uint16_t ib,
        uint16_t it,
        uint16_t in);

/* tick: compute a single sample of audio at position n */
int iter_block_tick(uint32_t *mem,
        uint16_t ib,
        uint16_t it,
        float in,
        int n);

/* trig/slice: get trigger and slice values at position */
int iter_block_trig(uint32_t *mem, uint16_t ib, int pos, uint32_t *trig);
int iter_block_slice(uint32_t *mem, uint16_t ib, int pos, uint32_t *slice);
/* next: similar to trig/slice, but for next entry */
int iter_block_next(uint32_t *mem, uint16_t ib, int pos, uint32_t *slice);

/* iter: get iterator address */
int iter_block_iter(uint32_t *mem, uint16_t ib, int pos, uint32_t *iter);

/* lookup: set up an array iterator to have a lookup table */
int iter_lookup(uint32_t *mem, uint16_t i, uint16_t lu);
