#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#include "context.h"
#include "ugen.h"

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
    array_init(mem, stk);

    /* store stack, zero page, and set addresses in temp */
    for (i = 0; i < bp; i++) {
        bitset_add(mem, bst, blk[i]);
    }

    /* interleave addresses into words, MSBs contain bitsets */
    /* zero page slot 0: temp | blocklist */
    /* zero page slot 1: main | stack */
    mem[zp] = bst << 16 | blist;
    mem[zp + 1] = bsm << 16 | stk;

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
    mem[ctx + 2] &= 0xFFFF;
    mem[ctx + 2] |= pstack << 16;

    return 0;
}

uint16_t context_pstack(uint32_t *mem, uint16_t ctx)
{
    /* zero page slot 2 MSB */
    return mem[ctx + 2] >> 16;
}

/* allocate a temporary block */
int context_mktemp(uint32_t *mem, uint16_t ctx, uint16_t *addr)
{
    uint16_t blist;
    uint16_t bst;
    uint16_t blk;

    blist = mem[ctx] & 0xFFFF;
    bst = (mem[ctx + 1] >> 16) & 0xFF;

    blk = blocklist_pop(mem, blist);
    if (blk == 0) return 1;
    bitset_add(mem, bst, blk);

    if (addr == NULL) return 2;

    *addr = block_to_word(bst, blk);

    return 0;
}

/* allocate a block */
int context_mkblock(uint32_t *mem, uint16_t ctx, uint16_t *addr)
{
    uint16_t blist;
    uint16_t bsm;
    uint16_t blk;

    blist = mem[ctx] & 0xFFFF;
    bsm = (mem[ctx + 1] >> 16) & 0xFF;

    blk = blocklist_pop(mem, blist);
    if (blk == 0) return 1;
    bitset_add(mem, bsm, blk);

    if (addr == NULL) return 2;

    *addr = block_to_word(bsm, blk);

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
