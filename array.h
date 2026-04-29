/* allocate array with current context */
int array_create(uint32_t *mem, uint16_t stk);

/* write value of an array a[p] = x */
int array_write(uint32_t *mem, uint16_t stk);

/* read value of an array x = a[p] */
int array_read(uint32_t *mem, uint16_t stk);

/* extracts value from a word slice */
uint32_t array_value(uint32_t *mem, uint32_t ws);
