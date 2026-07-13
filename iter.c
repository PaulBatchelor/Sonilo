#include <stdint.h>
#include <stddef.h>
#include "iter.h"
#include "mem.h"
#include "array.h"
#include "context.h"

/* iterator memory layout (TODO)
 */

/* iterator block memory layout
 *
 * Top (4 word) Top-level struct contains a tuple containing two
 * buffer addresses (slices, triggers), with the second
 * word used to store the current slice. The third word
 * contains a pointer to the next slices. The fourth word
 * is a cache for the next corresponding slice entry.
 *
 * Slices (1 block): The "slice" buffer is a 64-word block,
 * with each word containing an array slice, the result of
 * the array iterator.
 *
 * Triggers (2 words): The "trigger" buffer is 1-bit buffer
 * of size 64 (2 words * 32 bits/word = 64 bits). A value of
 * 1 indicates that a trigger happened at that particular sample.
 * Triggers signals are needed in the interpolate GSG component
 * to properly update interpolation values.
 *
 * Next Slices (1 block): Similar to the "slice" buffer,
 * but contains the next values following the currently
 * selected slice. This information is needed
 * for the interpolator to work (the interpolator needs
 * to know where to go).
 */

#define ITBLK_VALS(M, I) (M[I] & 0xFFFF);
#define ITBLK_TRIGS(M, I) ((M[I] >> 16) & 0xFFFF);
#define ITBLK_CURSLICE(M, I) M[I + 1]
#define ITBLK_NXTBLK(M, I) M[I + 2]
#define ITBLK_NXTSLICE(M, I) M[I + 3]


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

uint32_t iter_get(uint32_t *mem, uint16_t i)
{
    uint16_t idx, a;
    uint32_t slice;

    idx = mem[i] >> 16;
    a = mem[i + 1] & 0xFFFF;

    slice = 0;
    array_read_direct(mem,
            a,
            idx,
            &slice);
    return slice;
}

int iter_block_new(uint32_t *mem, uint16_t ctx, uint16_t *ib)
{
    int rc;
    uint16_t top, trigs, vals, nxt;
    int i;

    /* allocate top-level struct (4 words) */

    top = 0;
    rc = sonilo_alloc(mem, ctx, 4, &top);
    if (rc) return 1;

    /* allocate a block for values */
    vals = 0;
    rc = context_mkblock(mem, ctx, &vals);
    if (rc) return 2;

    /* allocate a block for next values */
    rc = context_mkblock(mem, ctx, &nxt);
    if (rc) return 5;

    /* allocate trigs (2 words) */
    trigs = 0;
    rc = sonilo_alloc(mem, ctx, 2, &trigs);
    if (rc) return 3;

    /* zero out data */
    mem[trigs] = mem[trigs + 1] = 0;
    for (i = 0; i < 64; i++) mem[vals + i] = mem[nxt + i] = 0;


    if (ib == NULL) return 4;

    /* first word stores (vals, trigs) tuple */
    mem[top] = vals | (trigs << 16);

    /* store the location of the next block */
    ITBLK_NXTBLK(mem, top) = nxt;

    /* zero out cur/nxt caches */
    ITBLK_CURSLICE(mem, top) = 0;
    ITBLK_NXTSLICE(mem, top) = 0;

    *ib = top;

    return 0;
}

/* tick: compute a single sample of audio at position n */
int iter_block_tick(uint32_t *mem,
        uint16_t ib,
        uint16_t it,
        float in,
        int n)
{
    uint32_t slice;
    uint8_t t;
    uint16_t vals, trigs, nxt;

    t = in > 0;
    t &= 1; /* extra precaution. Just the first bit */
    if (t) {
        ITBLK_CURSLICE(mem, ib) = iter_next(mem, it);
        ITBLK_NXTSLICE(mem, ib) = iter_get(mem, it);
    }

    /* retrieve current iterator value
     * store in slice buffer.
     */

    slice = ITBLK_CURSLICE(mem, ib);
    vals = ITBLK_VALS(mem, ib);
    mem[vals + n] = slice;
    slice = ITBLK_NXTSLICE(mem, ib);
    nxt = ITBLK_NXTBLK(mem, ib);
    mem[nxt + n] = slice;

    trigs = ITBLK_TRIGS(mem, ib);

    /* handle upper bits */
    if (n >= 32) {
        trigs += 1;
        n -= 32;
    }

    mem[trigs] &= ~(1 << n);
    mem[trigs] |= (t << n);

    return 0;
}

int iter_block_trig(uint32_t *mem, uint16_t ib, int pos, uint32_t *trig)
{
    uint16_t trigs;

    trigs = ITBLK_TRIGS(mem, ib);

    if (pos >= 32) {
        trigs += 1;
        pos -= 32;
    }

    if (trig == NULL) return 1;

    if (pos < 0 || pos >= 64) return 2;

    *trig = (mem[trigs] & (1 << pos)) >> pos;

    return 0;
}

int iter_block_slice(uint32_t *mem, uint16_t ib, int pos, uint32_t *slice)
{
    uint16_t vals;

    vals = ITBLK_VALS(mem, ib);

    if (slice == NULL) return 1;

    if (pos < 0 || pos >= 64) return 2;

    *slice = mem[vals + pos];

    return 0;
}

int iter_block_next(uint32_t *mem, uint16_t ib, int pos, uint32_t *slice)
{
    uint16_t vals;

    vals = ITBLK_NXTBLK(mem, ib);

    if (slice == NULL) return 1;

    if (pos < 0 || pos >= 64) return 2;

    *slice = mem[vals + pos];

    return 0;
}
