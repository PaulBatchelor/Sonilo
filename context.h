#define CTX_STACK(M,C) (M[C + 1] & 0xFFFF)
uint16_t context_init(uint32_t *mem, uint16_t blist);
int context_mktemp(uint32_t *mem, uint16_t ctx, uint16_t *addr);
int context_mkblock(uint32_t *mem, uint16_t ctx, uint16_t *addr);
void context_destroy(uint32_t *mem, uint16_t ctx);
int context_allocator_setup(uint32_t *mem, uint16_t ctx);
uint16_t context_allocator(uint32_t *mem, uint16_t ctx);
int context_pstack_setup(uint32_t *mem, uint16_t ctx);
uint16_t context_pstack(uint32_t *mem, uint16_t ctx);
