/* allocate array with current context */
int array_create(uint32_t *mem, uint16_t ctx, int wrdsz, uint16_t len, uint16_t *a);

/* set value of an array a[p] = x */
int array_set(uint32_t *mem, uint16_t a, uint16_t p, uint32_t x);

/* get value of an array x = a[p] */
int array_get(uint32_t *mem, uint16_t a, uint16_t p, uint32_t *x);

/* extracts value from a word slice */
uint32_t array_value(uint32_t *mem, uint32_t ws);
