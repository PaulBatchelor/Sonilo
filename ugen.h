#define MAX_PORTS 16
#define UGEN_BLKSZ 64
#define UGEN_BLOCK_MAX 126

enum {
    PORT_NONE = 0,
    PORT_BLOCK,
    PORT_CONSTANT
};

uint32_t pstack_param(int type, uint32_t data);
void pstack_init(uint32_t *mem, uint16_t p);
int pstack_push(uint32_t *mem, uint16_t p, uint32_t w);
int pstack_pop(uint32_t *mem, uint16_t p, uint32_t *w);
int pstack_drop(uint32_t *mem, uint16_t p);
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

/* ugen block: use a block of memory to store a sequence
 * of ugens */

/* create: allocate and initialize a ugen block, and pushes
 * the address onto the system stack */
int ugen_block_create(uint32_t *mem, uint16_t ctx);

/* append: appends a ugen address to the block, and possibly
 * allocates and links a new block if there is no more room
 * stack args: ugen block
 * returns: address of last block
 */
int ugen_block_append(uint32_t *mem, uint16_t ctx);

/* get: retrieves an address from a block */
int ugen_block_get(uint32_t *mem,
    uint16_t blk,
    uint16_t idx,
    uint16_t *out);

/* set: sets an address from a block */
int ugen_block_set(uint32_t *mem,
    uint16_t blk,
    uint16_t idx,
    uint16_t addr);

/* next: get next block entry */
int ugen_block_next(uint32_t *mem,
    uint16_t blk,
    uint16_t *out);
