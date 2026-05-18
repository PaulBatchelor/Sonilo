#include <stdint.h>
#include <stddef.h>
#include "iter.h"
#include "mem.h"
#include "array.h"

int iter_alloc(uint32_t *mem, uint16_t ctx, uint16_t *i)
{
    uint16_t p;
    int rc;

    /* allocate two words */
    p = 0;
    rc = sonilo_alloc(mem, ctx, 2, &p);
    if (rc) return 1;

    /* store result in i */
    if (i == NULL) return 2;

    *i = p;
    return 0;
}

/* init: create an empty iterator. it does nothing */
int iter_init(uint32_t *mem, uint16_t p)
{
    mem[p] = ITER_NONE;
    mem[p + 1] = 0;
    return 0;
}

/* array: set up an initialized iterator to point to an array */
int iter_array(uint32_t *mem, uint16_t i, uint16_t a)
{
    /* set iterator type/subtype to ARRAY/LOOP */
    mem[i] = ITER_ARRAY | (ITER_ARRAY_LOOP << 8);

    /* set pointer to array */
    mem[i + 1] = a;

    /* set index to be zero (zero out upper bits) */
    mem[i] &= 0xFFFF;
    return 0;
}

/* next: get the next value, returned as a slice
 * dereference it with array_value() */
uint32_t iter_next(uint32_t *mem, uint16_t i)
{
    uint16_t type;

    type = mem[i] & 0xFFFF;

    /* if empty (data short is 0), return 0 */

    if ((type & 0xFF) == ITER_NONE) return 0;

    /* handle ARRAY/LOOP */
    if ((type & 0xFF) == ITER_ARRAY) {
        if (((type >> 8) & 0xFF) == ITER_ARRAY_LOOP) {
            uint16_t idx;
            uint32_t slice;
            int rc;
            uint16_t a;

            /* get slice of current index */
            idx = mem[i] >> 16;
            a = mem[i + 1] & 0xFFFF;

            slice = 0;
            rc = array_read_direct(mem,
                    a,
                    idx,
                    &slice);

            if (rc) return 0;

            /* update index, wraparound if needed */
            idx++;
            idx %= array_length(mem, a);
            mem[i] &= 0xFFFF;
            mem[i] |= (idx << 16);

            return slice;
        }
    }

    return 0;
}

float iter_real(uint32_t *mem, uint16_t i)
{
    uint32_t slice;

    /* Note: eventually, this could be extended to include
     * differerent ways of converting to real depending on
     * the iterator type (TBD). For now, this is just recasting
     * the results of array_value() */
    slice = iter_next(mem, i);
    return (float)array_value(mem, slice);
} 
