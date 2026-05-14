enum {
    ITER_NONE = 0,
    ITER_ARRAY
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
