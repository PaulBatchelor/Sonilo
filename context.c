#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "array.h"

/* context: a set of components used together to form
 * a baseline setup for higher-level sonilo functionality,
 * formed from an initial global block list.
 * Components include: a zero page, with the first two
 * words containing two bitsets for tracking blocks
 * (main/temp), a pointer to the block list, and a local
 * argument stack.
 *
 * Returns address to zero page.
 */
uint16_t context_init(uint32_t *mem, uint16_t blist)
{
    int err;
    uint16_t zp;
    uint16_t bsm, bst;
    uint16_t stk;
    int blk[4];
    int bp;
    int i;

    bp = 0;
    err = 0x8000;

    /* create zero page */
    zp = blocklist_pop(mem, blist);
    if (zp == 0) return err | 1;
    blk[bp++] = zp;
    zp = block_to_word(blist, zp);
    zp = zero_page_init(mem, blist, zp);

    /* create main and temp bitsets */

    bsm = blocklist_pop(mem, blist);
    if (bsm == 0) return err | 2;
    blk[bp++] = bsm;
    bsm = block_to_word(blist, bsm);

    /* temp is in second half of block */
    bst = bsm + 32;

    bitset_init(mem, bsm);
    bitset_init(mem, bst);

    /* create stack */
    stk = blocklist_pop(mem, blist);
    if (stk == 0) return err | 3;
    blk[bp++] = stk;
    stk = block_to_word(blist, stk);
    barray_init(mem, stk);

    /* store stack, zero page, and set addresses in temp */
    for (i = 0; i < bp; i++) {
        bitset_add(mem, bst, blk[i]);
    }

    /* interleave addresses into words, MSBs contain bitsets */
    /* zero page slot 0: temp | blocklist */
    /* zero page slot 1: main | stack */
    mem[zp] = bst << 16 | blist;
    mem[zp + 1] = bsm << 16 | stk;

    /* VM + 8 megablocks  = (2^16 / 256) + 8 (reserved) = 264 */
    /* only set LSB */
    mem[zp + SLOT_UNIVERSE_FREE] &= ~0xFFFF;
    mem[zp + SLOT_UNIVERSE_FREE] |= 264;

    /* return zero page address */

    return zp;
}

int context_allocator_setup(uint32_t *mem, uint16_t ctx)
{
    uint16_t al;
    int err;

    /* instantiate allocator */
    al = 0;
    err = allocator_init(mem, ctx, &al);
    if (err) return 1;

    /* store address at slot 2 LSB in zero page */
    /* zero out LSB, and OR in the allocator */
    mem[ctx + 2] &= 0xFFFF0000;
    mem[ctx + 2] |= al;

    return 0;
}

uint16_t context_allocator(uint32_t *mem, uint16_t ctx)
{
    /* zero page slot 2 LSB */
    return mem[ctx + 2] & 0xFFFF;
}

int context_pstack_setup(uint32_t *mem, uint16_t ctx)
{
    uint16_t pstack;
    int err;
    /* allocate temp block for the stack */
    pstack = 0;
    err = context_mktemp(mem, ctx, &pstack);
    if (err) return 1;

    /* initialize pstack and reference counter */
    pstack_init(mem, pstack);
    /* NOTE: pstack base address is used for both RC and pstack */
    rc_init(mem, pstack);

    /* store pstack address at slot 2 MSB in zero page */
    /* zero out MSB, and OR in the pstack */
    mem[ctx + SLOT_PSTACK] &= 0xFFFF;
    mem[ctx + SLOT_PSTACK] |= pstack << 16;

    return 0;
}

uint16_t context_pstack(uint32_t *mem, uint16_t ctx)
{
    /* zero page slot 2 MSB */
    return mem[ctx + SLOT_PSTACK] >> 16;
}

/* allocate a temporary block */
int context_mktemp(uint32_t *mem, uint16_t ctx, uint16_t *addr)
{
    uint16_t blist;
    uint16_t bst;
    uint16_t blk;

    blist = mem[ctx] & 0xFFFF;
    bst = (mem[ctx + 1] >> 16) & 0xFFFF;

    blk = blocklist_pop(mem, blist);
    if (blk == 0) return 1;
    bitset_add(mem, bst, blk);

    if (addr == NULL) return 2;

    *addr = block_to_word(blist, blk);

    return 0;
}

/* allocate a block */
int context_mkblock(uint32_t *mem, uint16_t ctx, uint16_t *addr)
{
    uint16_t blist;
    uint16_t bsm;
    uint16_t blk;

    blist = mem[ctx] & 0xFFFF;
    bsm = (mem[ctx + 1] >> 16) & 0xFFFF;

    blk = blocklist_pop(mem, blist);
    if (blk == 0) {
        fprintf(stderr, "OUT OF BLOCKS\n");
        return 1;
    }
    bitset_add(mem, bsm, blk);

    if (addr == NULL) return 2;

    *addr = block_to_word(blist, blk);

    return 0;
}

/* frees memory used for context, preserves allocated main blocks */
void context_destroy(uint32_t *mem, uint16_t ctx)
{
    /* TODO: free blocks in temp set */
    /* TODO: retrieve BST (MSB in slot 0) */
    /* TODO: iterate over 32 words */
    /* TODO: free blocks in each word */
}

int context_pstack_sweep(uint32_t *mem, uint16_t ctx)
{
    uint16_t pstk;
    pstk = CTX_PARAM_STACK(mem, ctx);
    pstack_sweep(mem, pstk);
    return 0;
}

int context_ublock_setup(uint32_t *mem, uint16_t ctx)
{
    uint16_t stk;
    int rc;
    uint16_t blk;
    uint32_t val;
    stk = CTX_STACK(mem, ctx);

    /* allocate ugen block */
    rc = ugen_block_create(mem, ctx);
    if (rc) return 1;
    val = 0;
    rc = barray_pop(mem, stk, &val);
    blk = val;
    if (rc) return 2;

    /* store in context zero page slot */
    /* head/tail are the same */
    rc = context_ublock_head_set(mem, ctx, blk);
    if (rc) return 3;
    rc = context_ublock_tail_set(mem, ctx, blk);
    if (rc) return 4;
    return 0;
}

int context_ublock_tail_set(uint32_t *mem,
    uint16_t ctx,
    uint16_t tail)
{
    if (tail == 0) return 1;
    mem[ctx + SLOT_UGEN_BLOCK] &= ~0xFFFF;
    mem[ctx + SLOT_UGEN_BLOCK] |= tail;
    return 0;
}

int context_ublock_tail_get(uint32_t *mem,
    uint16_t ctx,
    uint16_t *tail)
{
    if (tail == NULL) return 1;
    *tail = mem[ctx + SLOT_UGEN_BLOCK] & 0xFFFF;
    return 0;
}

int context_ublock_head_get(uint32_t *mem,
    uint16_t ctx,
    uint16_t *head)
{
    if (head == NULL) return 1;
    *head = mem[ctx + SLOT_UGEN_BLOCK] >> 16;
    return 0;
}

int context_ublock_head_set(uint32_t *mem,
    uint16_t ctx,
    uint16_t head)
{
    if (head == 0) return 1;
    mem[ctx + SLOT_UGEN_BLOCK] &= 0xFFFF;
    mem[ctx + SLOT_UGEN_BLOCK] |= head<<16;
    return 0;
}

int context_sblock_init(uint32_t *mem, uint16_t ctx, uint8_t k)
{
    uint16_t b, stk;
    int rc;
    stk = CTX_STACK(mem, ctx);
    b = mem[ctx + SLOT_STAGING_BLOCK] & 0xFFFF;
    rc = staging_block_init(mem, b, k);
    if (rc) return 1;
    return barray_append(mem, stk, b);
}

int context_sblock_append(uint32_t *mem, uint16_t ctx)
{
    uint16_t b, stk;
    int rc;
    uint32_t x;
    b = mem[ctx + SLOT_STAGING_BLOCK] & 0xFFFF;
    stk = CTX_STACK(mem, ctx);
    rc = barray_pop(mem, stk, &x);
    if (rc) return 1;

    return staging_block_append(mem, b, x);
}

int context_sblock_copy(uint32_t *mem, uint16_t ctx)
{
    uint16_t a, b;
    int rc;
    uint16_t stk;

    b = mem[ctx + SLOT_STAGING_BLOCK] & 0xFFFF;
    stk = CTX_STACK(mem, ctx);
    a = 0;
    rc = staging_block_copy(mem, b, ctx, &a);
    if (rc) return 1;

    rc = barray_append(mem, stk, a);
    if (rc) return 2;

    return 0;
}

int context_sblock_setup(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t b;
    b = 0;
    rc = context_mktemp(mem, ctx, &b);
    if (rc) return 1;
    mem[ctx + SLOT_STAGING_BLOCK] = b;
    return 0;
}
