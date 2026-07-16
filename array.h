/* allocate array with current context */
int array_create_old(uint32_t *mem, uint16_t stk);
int array_create(uint32_t *mem, uint16_t ctx);

/* write value of an array a[p] = x, (x, p, a) */
int array_write(uint32_t *mem, uint16_t stk);

/* read value of an array x = a[p] */
int array_read(uint32_t *mem, uint16_t stk);

/* direct read of an array (no stack) */
int array_read_direct(uint32_t *mem, uint16_t a, uint16_t idx, uint32_t *slice);

/* extracts value from a word slice */
uint32_t array_value(uint32_t *mem, uint32_t ws);

/* extracts word slice value as floating point number, and
 * also has support for extended types */
float array_real(uint32_t *mem, uint32_t ws, uint8_t type);

/* get length of array */
uint16_t array_length(uint32_t *mem, uint16_t a);
