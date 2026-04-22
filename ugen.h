#define MAX_PORTS 16
#define UGEN_BLKSZ 64

enum {
    PARAM_BLOCK
};

enum {
    PORT_NONE = 0,
    PORT_BLOCK,
    PORT_CONSTANT
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
int pstack_sweep(uint32_t *mem, uint16_t p);

int ugen_create(uint32_t *mem,
        uint16_t ctx,
        uint16_t cmd,
        int nports,
        int sz,
        uint16_t *ugen);

int ugen_iport(uint32_t *mem, uint16_t ctx, uint16_t ugen, int port);
int ugen_oport(uint32_t *mem, uint16_t ctx, uint16_t ugen, int port);

void * ugen_state(uint32_t *mem, uint16_t ugen);

uint32_t* ugen_ports(uint32_t *mem, uint16_t ugen);
