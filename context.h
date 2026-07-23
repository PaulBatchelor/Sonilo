/* pstack/allocator share a slot */
#define SLOT_PSTACK 2
#define SLOT_ALLOC 2
#define SLOT_UGEN_BLOCK 3
#define SLOT_UNIVERSE_FREE 4 /* LSB */
#define SLOT_UGEN_BITS 4 /* MSB */
#define SLOT_STAGING_BLOCK 5 /* LSB */

#define CTX_STACK(M,C) (M[C + 1] & 0xFFFF)
#define CTX_PARAM_STACK(M,C) (M[C + SLOT_PSTACK] >> 16)
#define CTX_ALLOC(M,C) (M[C + SLOT_ALLOC] & 0xFFFF)
uint16_t context_init(uint32_t *mem, uint16_t blist);
int context_mktemp(uint32_t *mem, uint16_t ctx, uint16_t *addr);
int context_mkblock(uint32_t *mem, uint16_t ctx, uint16_t *addr);
void context_destroy(uint32_t *mem, uint16_t ctx);
int context_allocator_setup(uint32_t *mem, uint16_t ctx);
uint16_t context_allocator(uint32_t *mem, uint16_t ctx);
int context_pstack_setup(uint32_t *mem, uint16_t ctx);
uint16_t context_pstack(uint32_t *mem, uint16_t ctx);
int context_pstack_sweep(uint32_t *mem, uint16_t ctx);
int context_ublock_setup(uint32_t *mem, uint16_t ctx);
int context_ublock_tail_set(uint32_t *mem,
    uint16_t ctx,
    uint16_t tail);
int context_ublock_tail_get(uint32_t *mem,
    uint16_t ctx,
    uint16_t *tail);
int context_ublock_head_get(uint32_t *mem,
    uint16_t ctx,
    uint16_t *head);
int context_ublock_head_set(uint32_t *mem,
    uint16_t ctx,
    uint16_t head);

/* staging block context functions */

/* init: initialize sblock array of size 2^k */
int context_sblock_init(uint32_t *mem, uint16_t ctx, uint8_t k);

/* append: append value sblock */
int context_sblock_append(uint32_t *mem, uint16_t ctx);

/* copy: copy contents to newly allocated array */
int context_sblock_copy(uint32_t *mem, uint16_t ctx);

/* setup: configure and allocate the sblock */
int context_sblock_setup(uint32_t *mem, uint16_t ctx);
