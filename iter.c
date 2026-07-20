#include <stdint.h>
#include <stddef.h>
#include "iter.h"
#include "mem.h"
#include "array.h"
#include "context.h"
#include "gv.h"

/* iterator memory layout
 * Word 1: state/current index (MSB), type (type + subtype) (LSB)
 * Word 2: Array address (LSB), Lookup table address (MSB)
 */

#define ITER_TYPE(M, I) (M[I] & 0xFF)
#define ITER_ARRAY_LOC(I) (I + 1)
#define ITER_LOOKUP_LOC(I) (I + 1)
#define ITER_GET_ARRAY(M, I) M[ITER_ARRAY_LOC(I)]

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
#define ITBLK_NXTBLK_LOC(I) (I + 2)
#define ITBLK_NXTBLK(M, I) (M[ITBLK_NXTBLK_LOC(I)] & 0xFFFF)
#define ITBLK_NXTSLICE(M, I) M[I + 3]
#define ITBLK_ITER_LOC(I) (I + 2)
#define ITBLK_ITER(M, I) M[ITBLK_ITER_LOC(I)] >> 16;


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


static uint32_t array_next(uint32_t *mem, uint16_t i, uint16_t type)
{
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
        return array_next(mem, i, type);
    } else if ((type & 0xFF) == ITER_LOOKUP) {
        uint32_t slice, idx;
        uint16_t lu;
        int rc;
        uint8_t at;
        /* iterate through array and get index */
        slice = array_next(mem, i, type);
        idx = array_value(mem, slice);

        /* use index with lookup table */
        /* special case: if array is gesture vertex, only
         * use value portion */
        at = 0;
        rc = array_type_get(mem, mem[ITER_ARRAY_LOC(i)] & 0xFFFF, &at);
        if (rc) return 0;

        if (at == ARRAY_TYPE_GVERT) {
            idx = GV_VAL(idx);
        }

        lu = mem[ITER_LOOKUP_LOC(i)] >> 16;
        rc = array_read_direct(mem,
                lu,
                idx,
                &slice);

        if (rc) return 0;
        return slice;
    }

    return 0;
}

float iter_real_slice(uint32_t *mem, uint16_t i, uint32_t slice)
{
    uint8_t type;
    uint16_t a;
    int rc;

    /* determine which array to use based on iterator type */
    type = ITER_TYPE(mem, i);

    if (type == ITER_LOOKUP) {
        /* use lookup array (MSB) */
        a = ITER_GET_ARRAY(mem, i) >> 16;
    } else {
        /* use main array (LSB) */
        a = ITER_GET_ARRAY(mem, i) & 0xFFFF;
    }

    /* determine array type so it knows how to convert to real */
    type = 0;
    rc = array_type_get(mem, a, &type);

    if (rc) return -1;

    return array_real(mem, slice, type);
}

float iter_real(uint32_t *mem, uint16_t i)
{
    uint32_t slice;

    slice = iter_next(mem, i);

    /* TODO: consolide functions */
    return iter_real_slice(mem, i, slice);
} 

uint32_t iter_get(uint32_t *mem, uint16_t i)
{
    uint32_t idx, a;
    uint32_t slice;
    uint8_t type;

    idx = mem[i] >> 16;
    a = mem[i + 1] & 0xFFFF;
    /* determine which array to use based on iterator type */
    type = ITER_TYPE(mem, i);

    if (type == ITER_LOOKUP) {
        uint8_t at;
        /* first, retrieve the index from the main array */
        slice = 0;
        array_read_direct(mem,
                a,
                idx,
                &slice);
        idx = array_value(mem, slice);
        at = 0;
        array_type_get(mem, a, &at);
        /* if vertext, only extract value component */
        if (at == ARRAY_TYPE_GVERT) {
            idx = GV_VAL(idx);
        }
        /* set array to be look-up */
        a = ITER_GET_ARRAY(mem, i) >> 16;
    }

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
    mem[ITBLK_NXTBLK_LOC(top)] = nxt;

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

    /* store iter in MSB */
    mem[ITBLK_ITER_LOC(ib)] &= 0xFFFF;
    mem[ITBLK_ITER_LOC(ib)] |= it << 16;

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

int iter_block_iter(uint32_t *mem, uint16_t ib, uint16_t *iter)
{
    if (iter == NULL) return 1;
    *iter = mem[ITBLK_ITER_LOC(ib)] >> 16;
    return 0;
}

int iter_lookup(uint32_t *mem, uint16_t i, uint16_t lu)
{
    /* set the iterator type to be a lookup */
    mem[i] &= ~0xFF;
    mem[i] |= ITER_LOOKUP;

    /* store the lookup table (MSB)*/
    mem[ITER_LOOKUP_LOC(i)] &= 0xFFFF;
    mem[ITER_LOOKUP_LOC(i)] |= lu << 16;
    return 0;
}

uint16_t iter_get_array(uint32_t *mem, uint16_t i)
{
    return ITER_GET_ARRAY(mem, i) & 0xFFFF;
}
