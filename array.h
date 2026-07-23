#define ARRAY_TYPE_INT 0
#define ARRAY_TYPE_GVERT 1
#define ARRAY_TYPE_JI 2

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
/* array wordsize */
uint8_t array_wordsize(uint32_t *mem, uint16_t a);

/* type flags for array */
int array_type_set(uint32_t *mem, uint16_t a, uint8_t type);
int array_type_get(uint32_t *mem, uint16_t a, uint8_t *type);

/* staging block */

/* init(b, k): initialize staging block b as array with word size 2^k */
int staging_block_init(uint32_t *mem, uint16_t b, uint8_t k);
/* append(b, x): append value x to array */
int staging_block_append(uint32_t *mem, uint16_t b, uint32_t x);
/* copy(b, ctx): copy contents of staging block to an allocated array a */
int staging_block_copy(uint32_t *mem, uint16_t b, uint16_t ctx, uint16_t *a);
