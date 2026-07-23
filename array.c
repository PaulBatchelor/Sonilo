#include <stdint.h>
#include <stddef.h>
#include "array.h"
#include "mem.h"
#include "context.h"
#include "ji.h"
#include "gv.h"

/* array memory layout:
 * Header(1 word):
 *    - MSB: word size (3 bits, 2^k, 0 <= k <= 5).
 *    - LSB: length of array (16 bits, but really only 11 needed)
 * Data (n words): Packed values.
 */

int array_create(uint32_t *mem, uint16_t ctx)
{
    /* the old version required pushing the context address
     * onto the stack, but this would require extra steps
     * inside the word machine. Since the context can
     * get the stack, this function rework the logic so
     * only the context is used. */
    uint16_t stk;
    int rc;
    stk = CTX_STACK(mem, ctx);
    rc = barray_append(mem, stk, ctx);
    if (rc) return 1;

    /* with the context address pushed onto the stack, the old
     * array create function can be used */
    return array_create_old(mem, stk);
}

int array_create_old(uint32_t *mem, uint16_t stk)
{
    uint16_t ctx;
    uint8_t wrdsz;
    uint16_t len;
    uint16_t nbits;
    uint16_t nwords;
    int rc;
    uint16_t a;
    uint16_t i;
    uint32_t val;

    /* pop args: len.wrdsz context */

    val = 0;
    rc = barray_pop(mem, stk, &val);
    if (rc) return 6;
    ctx = val & 0xFFFF;

    rc = barray_pop(mem, stk, &val);
    if (rc) return 7;
    wrdsz = val & 0x7;
    val >>= 4;
    len = val & 0xFFFF;

    if (len == 0) return 1;

    if (wrdsz > 6) return 2;

    /* compute the number of words needed hold the bits */
    /* note: word size expressed as 2^k bits */
    nbits = (1 << wrdsz) * len;

    nwords = 0;
    while ((nwords << 5) < nbits) nwords++;

    /* add extra word for header */
    nwords++;

    if (nwords > 64) return 3;

    /* attempt to allocate words from context */
    a = 0;
    rc = sonilo_alloc(mem, ctx, nwords, &a);
    if (rc) return 4;

    /* initialize array */

    /* header: word size (2^k) | length */
    mem[a] = (wrdsz << 16) | len;

    /* zero out words */

    for (i = 1; i < nwords; i++) {
        mem[a + i] = 0;
    }

    /* push array address onto stack */

    rc = barray_append(mem, stk, a);

    if (rc) return 8;

    return 0;
}

static int awrite(uint32_t *mem,
        uint16_t a,
        uint16_t p,
        uint32_t x)
{
    uint16_t len, wsz;
    uint16_t ow, ob;
    uint32_t m;
    /* extract length and word size from header */

    len = mem[a] & 0xFFFF;
    wsz = (mem[a] >> 16) & 0x7;

    /* bounds checking */

    if (p >= len) return 4;

    if (wsz > 5) return 5;

    /* calculate word and bit offsets */

    /* O_w = p * 2^{5 - k} */
    ow = p >> (5 - wsz);
    /* O_b = p * 2^k - 32*O_w */
    ob = p * (1 << wsz) - (ow << 5);

    /* write bits to word */

    /* turn word offset into memory offset */
    ow += a + 1;

    /* calculate mask: (2^(2^k) - 1) * 2^{O_b} */

    m = 0xFFFFFFFF;
    if (wsz < 5) m = ((1 << (1 << wsz)) - 1) << ob;

    /* clear bits */
    mem[ow] &= ~m;

    /* write bits */
    mem[ow] |= (x << ob) & m;
    return 0;
}

/* write value of an array a[p] = x. args: x p a */
int array_write(uint32_t *mem, uint16_t stk)
{
    uint16_t a, p;
    uint32_t x;
    int rc;
    uint32_t v;

    if (mem[stk] < 3) return 1;

    /* args: x p a */
    v = 0;
    rc = barray_pop(mem, stk, &v);
    if (rc) return 1;
    a = v & 0xFFFF;

    rc = barray_pop(mem, stk, &v);
    if (rc) return 2;
    p = v & 0xFFFF;

    rc = barray_pop(mem, stk, &v);
    if (rc) return 3;
    x = v;

    rc = awrite(mem, a, p, x);

    if (rc) return rc;

    /* push address onto stack again */

    rc = barray_append(mem, stk, a);
    if (rc) return 6;

    return 0;
}

static uint32_t wordslice(uint16_t addr, uint16_t start, uint16_t end)
{
    return addr | start << 16 | end << 21;
}

static int aread(uint32_t *mem, uint16_t a, uint16_t p, uint32_t *slice)
{
    uint16_t len, k;
    uint16_t ob, ow;

    /* extract length and word size from header */
    len = mem[a] & 0xFFFF;
    k = (mem[a] >> 16) & 0x7;

    /* bounds checking */
    if (p >= len) return 3;

    /* calculate word and bit offsets */
    ow = p >> (5 - k);
    ob = p * (1 << k) - (ow << 5);

    /* generate word slice, push to stack */

    ow += a + 1;
    if (slice == NULL) return 5;

    *slice = wordslice(ow, ob, ob + (1 << k) - 1);
    return 0;
}

/* read value of an array x = a[p] */
int array_read(uint32_t *mem, uint16_t stk)
{
    int rc;
    uint32_t v;
    uint16_t a, p;
    uint32_t slice;

    v = 0;
    /* pop args: p, a */
    rc = barray_pop(mem, stk, &v);
    if (rc) return 1;
    a = v & 0xFFFF;

    rc = barray_pop(mem, stk, &v);
    if (rc) return 2;
    p = v & 0xFFFF;

    rc = aread(mem, a, p, &slice);
    if (rc) return rc;
    /* push array address back onto stack */
    rc = barray_append(mem, stk, a);
    if (rc) return 5;

    rc = barray_append(mem, stk, slice);
    if (rc) return 4;

    return 0;
}

/* NOTE: only use this for reading integer values. For
 * extended types, use array_real() */
uint32_t array_value(uint32_t *mem, uint32_t ws)
{
    uint16_t addr;
    uint8_t start, end;
    uint32_t mask;

    addr = ws & 0xFFFF;
    ws >>= 16;

    start = ws & 0x1F;
    ws >>= 5;

    end = ws & 0x1F;
    ws >>= 5;

    if (start > end) {
        uint16_t tmp;
        /* TODO: xor trick */
        tmp = start;
        start = end;
        end = tmp;
    }

    mask = 0xFFFFFFFF;
    if ((end - start) < 31) {
        mask = ((1 << (end - start + 1)) - 1) << start;
    }

    return (mem[addr] & mask) >> start;
}

int array_read_direct(uint32_t *mem, uint16_t a, uint16_t idx, uint32_t *slice)
{
    return aread(mem, a, idx, slice);
}

uint16_t array_length(uint32_t *mem, uint16_t a)
{
    return mem[a] & 0xFFFF;
}

uint8_t array_wordsize(uint32_t *mem, uint16_t a)
{
    return (mem[a] >> 16) & 0x7;
}

float array_real(uint32_t *mem, uint32_t ws, uint8_t type)
{
    float out;
    uint32_t ival;

    out = 0;

    switch (type) {
        case ARRAY_TYPE_JI:
            ival = array_value(mem, ws);
            out = ji_real(ival);
            break;
        case ARRAY_TYPE_GVERT:
            ival = array_value(mem, ws);
            out = GV_VAL(ival);
            break;
        case ARRAY_TYPE_INT:
        default:
            ival = array_value(mem, ws);
            out = (float)ival;
            break;
    }

    
    return out;
}

int array_type_set(uint32_t *mem, uint16_t a, uint8_t type)
{
    /* 5 bits for type, after 3-bit k-len */
    mem[a] &= ~(31 << (3 + 16));
    mem[a] |= (type & 31) << (3 + 16);
    return 0;
}

int array_type_get(uint32_t *mem, uint16_t a, uint8_t *type)
{
    if (type == NULL) return 1;
    *type = (mem[a] >> (16 + 3)) & 31;
    return 0;
}

int staging_block_init(uint32_t *mem, uint16_t b, uint8_t k)
{
    uint8_t i;
    mem[b] = (k & 0x7) << 16;
    for (i = 1; i < 64; i++) mem[b + i] = 0;
    return 0;
}

int staging_block_append(uint32_t *mem, uint16_t b, uint32_t x)
{
    uint16_t len;
    int rc;
    /* increment size */
    len = mem[b] & 0xFFFF;
    mem[b] &= ~0xFFFF;
    mem[b] |= len + 1;

    /* write to last index item */
    rc = awrite(mem, b, len, x);
    if (rc) return 1;

    return 0;
}

int staging_block_copy(uint32_t *mem, uint16_t b, uint16_t ctx, uint16_t *pa)
{
    uint16_t len, i;
    int rc;
    uint16_t a;
    uint8_t at;
    uint16_t stk;
    uint8_t wsz;
    uint32_t x;

    if (pa == NULL) return 1;

    stk = CTX_STACK(mem, ctx);

    len = array_length(mem, b);
    wsz = array_wordsize(mem, b);

    a = 0;

    /* push array args on to stack */
    rc = barray_append(mem, stk, (len << 4) | wsz);
    if (rc) return 1;

    rc = array_create(mem, ctx);
    if (rc) return 2;

    rc = barray_pop(mem, stk, &x);
    if (rc) return 3;
    a = x;

    at = 0;
    rc = array_type_get(mem, b, &at);
    if (rc) return 4;
    rc = array_type_set(mem, a, at);
    if (rc) return 5;

    for (i = 0; i < len; i++) {
        uint32_t x;
        x = 0;
        rc = aread(mem, b, i, &x);
        if (rc) return 3;
        /* output is a slice, decode it into a value */
        x = array_value(mem, x);
        rc = awrite(mem, a, i, x);
        if (rc) return 4;
    }

    *pa = a;

    return 0;
}
