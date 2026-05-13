#include <stdint.h>
#include "iter.h"

/* init: create an empty iterator. it does nothing */
int iter_init(uint32_t *mem, uint16_t p)
{
    /* TODO: zero out bits */
    return 1;
}

/* array: set up an initialized iterator to point to an array */
int iter_array(uint32_t *mem, uint16_t i, uint16_t a)
{
    /* TODO: set iterator type/subtype to ARRAY/LOOP */
    /* TODO: set pointer to array */
    /* TODO: set index to be zero */
    return 1;
}

/* next: get the next value, returned as a slice
 * dereference it with array_value() */
uint32_t iter_next(uint32_t *mem, uint16_t i)
{
    /* TODO: if empty (data short is 0), return 0 */

    /* TODO: handle ARRAY/LOOP */
    /* TODO: get slice of current index */
    /* TODO: update index, wraparound if needed */
    /* TODO: return slice */
    return 0;
}
