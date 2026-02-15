enum {
    PARAM_BLOCK
};

uint32_t pstack_param(int type, uint32_t data);
void pstack_init(uint32_t *mem, uint16_t p);
int pstack_push(uint32_t *mem, uint16_t p, uint32_t w);
int pstack_pop(uint32_t *mem, uint16_t p, uint32_t *w);
int pstack_dup(uint32_t *mem, uint16_t p);
int pstack_swap(uint32_t *mem, uint16_t p);
int pstack_rot(uint32_t *mem, uint16_t p);
int pstack_hold(uint32_t *mem, uint16_t p);
int pstack_unhold(uint32_t *mem, uint16_t p);
