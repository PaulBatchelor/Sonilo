#include "sonilo.h"
#include "ins.h"
#include "mem.h"
#include "ugen.h"
#include "context.h"

#define MAX_PORTS 16

struct sonilo {
    /* linear memory */
    uint32_t mem[65536];

    /* read/write register */
    uint32_t rw;

    /* a/b cursors */
    uint16_t *cur;
    uint16_t a, b;

    /* pointer to blocklist */
    uint16_t blocklist;

    /* instruction map lookup */
    instr_map instr;
};

void sonilo_init(sonilo *s)
{
    uint32_t i;

    /* zero out memory */

    for (i = 0; i < 65536; i++) {
        s->mem[i] = 0;
    }

    s->rw = 0;

    s->a = s->b = 0;
    s->cur = &s->a;

    s->blocklist = 0;

    /* setup blocklist */

    blocklist_init(s->mem, s->blocklist);

    /* TODO: load ugens */
}

void sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s)
{
    ctx->context = context_init(s->mem, s->blocklist);
    ctx->s = s;

    /* set up allocator */
    /* TODO: error checking */
    context_allocator_setup(ctx->s->mem, ctx->context);
    ctx->allocator = context_allocator(ctx->s->mem, ctx->context);

    /* TODO: error checking */
    context_pstack_setup(ctx->s->mem, ctx->context);
    ctx->pstack = context_pstack(ctx->s->mem, ctx->context);
}

void sonilo_ctx_destroy(sonilo_ctx *ctx)
{
    context_destroy(ctx->s->mem, ctx->context);
}

size_t sonilo_sizeof(void)
{
    return sizeof(sonilo);
}

/* an abstraction to handle the inner details of allocating
 * memory using the buddy slot allocator */
int sonilo_alloc(sonilo_ctx *ctx, int sz, uint16_t *p)
{
    int rc;
    uint32_t stk, bud, k, base;
    int offset;
    uint32_t *mem;

    /* basic bounds checking: this is only for sub-block non-zero sizes */
    if (sz <= 0 || sz >= 64) return 1;

    mem = ctx->s->mem;
    /* call buddy slot allocator (stack: base, buddy args) */
    rc = allocator_alloc(mem, ctx->allocator, sz); 

    /* error handling */
    if (rc) return 2;

    /* call buddy allocator (stack: base, local offset) */
    /* NOTE: there isn't a great abstraction for this yet.
     * Ideally, stack operations would be implicit, but that
     * hasn't been built out yet.
     */

    /* get stack (slot 1 LSB) */
    stk = mem[ctx->context + 1] & 0xFFFF;

    /* pop bud and k params */
    rc = array_pop(mem, stk, &bud);
    if (rc) return 3;
    rc = array_pop(mem, stk, &k);
    if (rc) return 4;

    /* call buddy allocator */
    offset = mem_alloc(mem, bud, k);

    if (offset < 0) return 5;

    rc = array_pop(mem, stk, &base);
    if (rc) return 6;

    if (p == NULL) return 7;

    *p = offset + base;

    return 0;
}

int sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, uint16_t ukey, int nports, int sz)
{
    int rc;
    uint16_t top, ports, state;
    int cmd;
    uint32_t *mem;
    mem = ctx->s->mem;

    /* TODO: setting up this ugen should be moved to another
     * more low-level function */

    /* BEGIN low-level ugen set-up */
    top = ports = state = 0;
    /* allocate top struct (2 words) */
    rc = sonilo_alloc(ctx, 2, &top);
    if (rc) return 1;
    
    /* allocate and initialize port array (N words) */
    if (nports > MAX_PORTS || nports <= 0) return 2;
    rc = sonilo_alloc(ctx, nports, &ports);
    if (rc) return 2;

    /* allocate user data (SZ words) */
    rc = sonilo_alloc(ctx, sz, &state);
    if (rc) return 3;

    /* resolve command key to function index pointer (instr_map_index) */
    cmd = instr_map_index(&ctx->s->instr, ukey);

    if (cmd < 0) return 4;

    /* store ugen addresses in memory */
    mem[top] = (state << 16) | ports;
    mem[top + 1] = cmd;

    /* END low-level ugen set-up */

    if (u == NULL) return 5;

    /* store information in ugen struct */
    u->ctx = ctx;
    u->data.top = top;
    u->data.ports = &mem[ports];
    u->data.state = &mem[state];
    u->data.cmd = cmd;

    return 0;
}

int sonilo_iport(sonilo_ugen *u, int port)
{
    uint32_t w;
    int rc;

    /* pop param from stack */
    w = 0;
    rc = pstack_pop(u->ctx->s->mem, u->ctx->pstack, &w);
    if (rc) return 1;

    /* set param to port number */
    u->data.ports[port] = w;
    return 0;
}

static sonilo_port new_block_port(sonilo_ctx *ctx)
{
    sonilo_port p;
    uint16_t b;

    /* allocate block to main */
    context_mkblock(ctx->s->mem, ctx->context, &b);
    /* TODO: error checking */

    /* sonilo_port_block */
    p = sonilo_port_block(ctx->s->mem, b);
    return p;
}

int sonilo_oport(sonilo_ugen *u, int port)
{
    uint32_t w;
    sonilo_port p;
    uint32_t *mem;
    int rc;

    mem = u->ctx->s->mem;
    /* allocate a block inside of a port */
    p = new_block_port(u->ctx);

    /* convert to param word data */
    w = sonilo_port_to_word(mem, &p);

    /* push param word onto stack */
    rc = pstack_push(mem, u->ctx->pstack, w);
    if (rc) return 1;

    /* save param word to port */
    u->data.ports[port] = w;
    return 0;
}

int sonilo_constant(sonilo_ctx *ctx, float c)
{
    sonilo_port p;
    uint32_t w;
    int rc;
    uint32_t *mem;

    mem = ctx->s->mem;

    /* convert constant to param */
    p = sonilo_port_constant(c);
    w = sonilo_port_to_word(mem, &p);

    /* push param onto stack */
    rc = pstack_push(mem, ctx->pstack, w);
    if (rc) {
        /* err 1 or 2: overflow or RC address not found for block */
        if (rc < 3) return rc;
        /* unknown error */
        else return 3;
    }

    return 0;
}

int sonilo_flush(sonilo_ctx *ctx)
{
    return pstack_sweep(ctx->s->mem, ctx->pstack);
}

void sonilo_ugen_compute(sonilo_ugen *u)
{
    instr_func f;
    /* retrieve callback from sonilo VM (instr_map_get) */
    f = instr_map_entry(&u->ctx->s->instr, u->data.cmd);
    if (f == NULL) return;

    /* call, store result in rw */
    u->ctx->s->rw = f(u->ctx->s->mem, u->data.top);
}

uint16_t sonilo_key(const char *key)
{
    return instr_key(key);
}

uint32_t sonilo_srate(uint32_t *mem)
{
    /* fixed 44.1kHZ samplerate (for now) */
    return 44100;
}

uint16_t sonilo_command(sonilo *s, uint16_t key, instr_func func)
{
    instr_map_set(&s->instr, key, func);
    return 0;
}

/* block: gets block at port. errors if port is not a block */
int sonilo_ugen_block(uint32_t *mem,
        uint16_t ugen,
        int portnum,
        float **block)
{
    sonilo_port p;
    uint32_t *ports;

    if (mem == NULL) return 3;

    /* dereference ports list (LSB at word 0) */
    ports = &mem[mem[ugen] & 0xFFFF];
    p = sonilo_port_from_word(mem, ports[portnum]);
    /* if not a block, return error */
    if (p.type != PORT_BLOCK) return 1;

    /* TODO: am I null checking the right thing? */
    if (block == NULL) return 2;

    /* store pointer */
    *block = p.data.block.block;
    return 0;
}

/* get a ugen from a sonilo memory location */
int sonilo_ugen_get(uint32_t *mem, sonilo_ugen_data *u, uint16_t p)
{
    uint16_t m_ports, m_state, cmd;
    /* ugen components: ports, data, opcode */
    m_ports = mem[p] & 0xFFFF;
    m_state = (mem[p] >> 16) & 0xFFFF;
    cmd = mem[p + 1];

    u->ports = &mem[m_ports];
    u->state = &mem[m_state];
    u->cmd = cmd;
    u->top = p;

    return 0;
}

uint32_t *sonilo_mem(sonilo *s)
{
    if (s == NULL) return NULL;
    return s->mem;
}

int sonilo_symbol(sonilo_ctx *ctx, const char *sym)
{
    uint16_t key;
    uint16_t stk;
    int rc;
    uint32_t *mem;

    key = sonilo_key(sym);
    mem = ctx->s->mem;

    stk = mem[ctx->context + 1] & 0xFFFF;
    rc = array_append(mem, stk, key);

    if (rc) return 1;

    return 0;
}

int sonilo_pop(sonilo_ctx *ctx, uint32_t *w)
{
    int rc;
    uint16_t stk;
    uint32_t *mem;

    mem = ctx->s->mem;
    stk = mem[ctx->context + 1] & 0xFFFF;
    rc = array_pop(mem, stk, w);
    if (rc) return 1;

    return 0;
}

int sonilo_peak(sonilo_ctx *ctx, uint32_t *w)
{
    int rc;
    uint16_t stk;
    uint32_t *mem;

    mem = ctx->s->mem;
    stk = mem[ctx->context + 1] & 0xFFFF;
    rc = array_peak(mem, stk, w);
    if (rc) return 1;

    return 0;
}

int sonilo_set(sonilo *s, uint32_t w)
{
    if (s == NULL) return 1;
    s->rw = w;
    return 0;
}

int sonilo_go(sonilo *s)
{
    if (s == NULL) return 1;
    *s->cur = s->rw & 0xFFFF;
    return 0;
}

int sonilo_read(sonilo *s)
{
    if (s == NULL) return 1;
    s->rw = s->mem[*s->cur];
    return 0;
}

/* call a pre-resolved key to a subroutine directly */
int sonilo_call_direct(sonilo *s)
{
    uint32_t rw;
    instr_func f;
    rw = s->rw;
   
    f = instr_map_entry(&s->instr, rw & 0xFFFF);

    if (f == NULL) return 1;

    s->rw = f(s->mem, rw >> 16);

    return 0;
}

int sonilo_call(sonilo *s)
{
    int rc;

    rc = instr_ex(s->mem, &s->instr, s->rw, &s->rw);

    if (rc) return 1;

    return 0;
}

int sonilo_ugen_create(sonilo_ctx *ctx)
{
    uint32_t key;
    int rc;
    sonilo *s;

    s = ctx->s;
    /* peak key from stack, it will be needed for ugen init */
    rc = sonilo_pop(ctx, &key);
    if (rc) return 1;

    /* pack bits: CMD | INSTR */
    rc = sonilo_set(s, (ctx->context << 16) | (key & 0xFFFF));
    if (rc) return 2;

    /* call init function */
    rc = sonilo_call(s);
    if (rc) return 3;

    return 0;
}
