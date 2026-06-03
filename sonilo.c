#include <stdlib.h>
#include "sonilo.h"
#include "mem.h"
#include "ugen.h"
#include "context.h"
#include "ins.h"
#include "wm.h"

int sonilo_load_ugens(sonilo *s);

struct sonilo_host {
    /* instruction map */
    instr_func func[MAX_INSTR];
    instr_map map;
};

struct sonilo_vm {
    /* linear memory (256 megablocks) */
    uint32_t mem[0x10000];

    /* system block: 1 megablock */

    /* w_0: read/write register */
    uint32_t rw;

    /* w_1: errors */
    uint32_t err;

    /* w_2: a/b cursors (1 word) */
    uint16_t cursor, alt;

    /* w_3: switch/blocklist */
    /* switch flag indicating which cursor (short) */
    uint16_t swap;

    /* pointer to blocklist (short) */
    uint16_t blocklist;

    /* instruction keys + size (should fit into remaining
     * megablock. 252 words, 251 entries + size) */
    uint32_t nentries;
    uint32_t key[MAX_INSTR];
};

struct sonilo {
    /* universe: main memory */
    uint32_t *universe;

    /* virtual machine components */
    sonilo_vm *vm;
    /* linear memory */
    uint32_t *mem;

    /* read/write register */
    uint32_t *rw;

    /* host components */
    sonilo_host host;

    /* word machine: virtual device to interact with sonilo  */
    word_machine *wm;
};

int sonilo_init(sonilo *s)
{
    int rc;

    s->mem = sonilo_vm_mem(s->vm);
    s->rw = sonilo_vm_rw_ptr(s->vm);

    /* setup blocklist */

    rc = sonilo_load_ugens(s);

    if (rc) return 2;

    return 0;
}

int sonilo_vm_init(sonilo_vm *vm)
{
    uint32_t i;
    int rc;

    vm->rw = 0;
    for (i = 0; i < 0x10000; i++) vm->mem[i] = 0;
    vm->cursor = 0;
    vm->alt = 0;
    vm->swap = 0;
    vm->err = 0;

    for (i = 0; i < MAX_INSTR; i++) {
        vm->key[i] = 0;
    }
    vm->nentries = 0;

    /* setup blocklist */

    vm->blocklist = 0;
    rc = blocklist_init(vm->mem, vm->blocklist);
    if (rc) return 1;

    return 0;
}

size_t sonilo_vm_sizeof(void)
{
    return sizeof(sonilo_vm);
}

int sonilo_ctx_init(sonilo_ctx *ctx, sonilo *s)
{
    int rc;

    ctx->context = context_init(s->mem, sonilo_vm_blocklist(s->vm));
    ctx->s = s;

    /* set up allocator */
    rc = context_allocator_setup(ctx->s->mem, ctx->context);
    if (rc) return 1;

    ctx->allocator = context_allocator(ctx->s->mem, ctx->context);

    /* set up parameter stack */
    rc = context_pstack_setup(ctx->s->mem, ctx->context);
    if (rc) return 2;
    ctx->pstack = context_pstack(ctx->s->mem, ctx->context);

    /* set up ugen block */
    rc = context_ublock_setup(ctx->s->mem, ctx->context);
    if (rc) return 3;

    return 0;
}

int sonilo_ctx_destroy(sonilo_ctx *ctx)
{
    context_destroy(ctx->s->mem, ctx->context);
    return 0;
}

size_t sonilo_sizeof(void)
{
    return sizeof(sonilo);
}

/* an abstraction to handle the inner details of allocating
 * memory using the buddy slot allocator */
int sonilo_alloc(uint32_t *mem, uint16_t ctx, int sz, uint16_t *p)
{
    int rc;
    uint32_t stk, bud, k, base;
    int offset;
    uint16_t a;

    /* basic bounds checking: this is only for sub-block non-zero sizes */
    if (sz <= 0 || sz >= 64) return 1;

    a = CTX_ALLOC(mem, ctx);
    /* call buddy slot allocator (stack: base, buddy args) */
    rc = allocator_alloc(mem, a, sz); 

    /* error handling */
    if (rc) return 2;

    /* call buddy allocator (stack: base, local offset) */
    /* NOTE: there isn't a great abstraction for this yet.
     * Ideally, stack operations would be implicit, but that
     * hasn't been built out yet.
     */

    /* get stack (slot 1 LSB) */
    stk = CTX_STACK(mem, ctx);

    /* pop bud and k params */
    rc = barray_pop(mem, stk, &bud);
    if (rc) return 3;
    rc = barray_pop(mem, stk, &k);
    if (rc) return 4;

    /* call buddy allocator */
    offset = mem_alloc(mem, bud, k);

    if (offset < 0) return 5;

    rc = barray_pop(mem, stk, &base);
    if (rc) return 6;

    if (p == NULL) return 7;

    *p = offset + base;

    return 0;
}

int sonilo_ugen_init(sonilo_ctx *ctx, sonilo_ugen *u, uint16_t ukey, int nports, int sz)
{
    /* TODO: deprecate? I don't see this being all that useful */
    return 1;
#if 0
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
    rc = sonilo_alloc(mem, ctx->context, 2, &top);
    if (rc) return 1;
    
    /* allocate and initialize port array (N words) */
    if (nports > MAX_PORTS || nports <= 0) return 2;
    rc = sonilo_alloc(mem, ctx->context, nports, &ports);
    if (rc) return 3;

    /* allocate user data (SZ words) */
    rc = sonilo_alloc(mem, ctx->context, sz, &state);
    if (rc) return 4;

    /* resolve command key to function index pointer (instr_map_index) */
    cmd = instr_map_index(&ctx->s->instr, ukey);

    if (cmd < 0) return 5;

    /* store ugen addresses in memory */
    mem[top] = (state << 16) | ports;
    mem[top + 1] = cmd;

    /* END low-level ugen set-up */

    if (u == NULL) return 6;

    /* store information in ugen struct */
    u->ctx = ctx;
    u->data.top = top;
    u->data.ports = &mem[ports];
    u->data.state = &mem[state];
    u->data.cmd = cmd;

    return 0;
#endif
}

/* TODO: OUTDATED. re-work to use ugen_iport */
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

/* TODO: OUTDATED. rework to use ugen_oport under the hood */
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
#if 0
    instr_func f;
    /* retrieve callback from sonilo VM (instr_map_get) */
    f = instr_map_entry(&u->ctx->s->instr, u->data.cmd);
    if (f == NULL) return;

    /* call, store result in rw */
    *u->ctx->s->rw = f(u->ctx->s->mem, u->data.top);
#endif
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
    sonilo_host_cfunc(&s->host, key, func);
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
    rc = barray_append(mem, stk, key);

    if (rc) return 1;

    return 0;
}

int sonilo_pop(sonilo_ctx *ctx, uint32_t *w)
{
    int rc;
    uint16_t stk;
    uint32_t *mem;

    mem = ctx->s->mem;
    stk = CTX_STACK(mem, ctx->context);
    rc = barray_pop(mem, stk, w);
    if (rc) return 1;

    return 0;
}

int sonilo_push(sonilo_ctx *ctx, uint32_t w)
{
    int rc;
    uint16_t stk;
    uint32_t *mem;

    mem = ctx->s->mem;
    stk = CTX_STACK(mem, ctx->context);
    rc = barray_append(mem, stk, w);
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
    rc = barray_peak(mem, stk, w);
    if (rc) return 1;

    return 0;
}

int sonilo_set(sonilo *s, uint32_t w)
{
    if (s == NULL) return 1;
    *s->rw = w;
    return 0;
}

int sonilo_get(sonilo *s, uint32_t *w)
{
    if (s == NULL) return 1;
    *w = *s->rw;
    return 0;
}

int sonilo_go(sonilo *s)
{
    uint32_t rw;
    if (s == NULL) return 1;
    rw = sonilo_vm_rw_get(s->vm);
    rw &= 0xFFFF;
    sonilo_vm_cursor_set(s->vm, rw);
    return 0;
}

int sonilo_read(sonilo *s)
{
    if (s == NULL) return 1;
    sonilo_vm_read(s->vm);
    return 0;
}

/* call a pre-resolved key to a subroutine directly */
int sonilo_call_direct(sonilo *s)
{
    uint32_t rw;
    instr_func f;
    rw = *s->rw;
   
    f = instr_map_entry(&s->host.map, rw & 0xFFFF);

    if (f == NULL) return 1;

    *s->rw = f(s->mem, rw >> 16);

    return 0;
}

int sonilo_call(sonilo *s)
{
    int rc;

    rc = sonilo_host_ex(&s->host, s->mem, *s->rw, s->rw);

    if (rc) return 1;

    return 0;
}

int sonilo_ugen_create(sonilo_ctx *ctx)
{
    uint32_t key;
    int render;
    uint32_t rw;
    int rc;
    sonilo *s;

    s = ctx->s;
    /* pop key from stack */
    rc = sonilo_pop(ctx, &key);
    if (rc) return 1;

    /* look up entry for DSP callback (alt key) */
    render = instr_map_index(&ctx->s->host.map, sonilo_alt(key));
    if (render < 0) return 6;

    /* push function index of render callback to stack */
    rc = sonilo_push(ctx, render);
    if (rc) return 7;

    /* pack bits: CMD | INSTR */
    rc = sonilo_set(s, (ctx->context << 16) | (key & 0xFFFF));
    if (rc) return 2;

    /* call init function */
    rc = sonilo_call(s);
    if (rc) return 3;

    /* check return code of init function */
    rw = 0;
    rc = sonilo_get(s, &rw);
    if (rc) return 4;
    if (rw) return 5;

    return 0;
}

uint16_t sonilo_alt(uint16_t key)
{
    return key | 1;
}

int sonilo_ugen_register(sonilo *s,
        const char *sym,
        instr_func init,
        instr_func render)
{
    uint16_t key;
    int rc;

    key = sonilo_key(sym);

    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}

int sonilo_hold(sonilo_ctx *ctx, uint32_t *w)
{
    uint16_t pstk;
    uint32_t *mem;
    int rc;

    if (w == NULL) return 2;

    mem = ctx->s->mem;
    pstk = context_pstack(mem, ctx->context);
    rc = pstack_hold(mem, pstk);

    if (rc) return 1;

    rc = pstack_pop(mem, pstk, w);
    if (rc) return 3;

    return 0;
}

int sonilo_unhold(sonilo_ctx *ctx, uint32_t w)
{
    uint16_t pstk;
    uint32_t *mem;
    int rc;

    mem = ctx->s->mem;
    pstk = context_pstack(mem, ctx->context);

    rc = pstack_push(mem, pstk, w);
    if (rc) return 1;

    rc = pstack_unhold(mem, pstk);
    if (rc) return 2;

    return 0;
}

int sonilo_ppush(sonilo_ctx *ctx, uint32_t w)
{
    uint16_t pstk;
    uint32_t *mem;
    int rc;

    mem = ctx->s->mem;
    pstk = context_pstack(mem, ctx->context);

    rc = pstack_push(mem, pstk, w);
    if (rc) return 1;

    return 0;
}

int sonilo_process(sonilo_ctx *ctx)
{
    int rc;
    uint16_t block, next, pos;
    uint32_t *mem;

    mem = ctx->s->mem;
    /*  initialize local block and index variables */
    block = 0;
    rc = context_ublock_head_get(mem, ctx->context, &block);
    if (rc) return 1;

    next = 0;
    rc = ugen_block_next(mem, block, &next);
    if (rc) return 2;

    /* loop (treat 0 as NULL) */
    pos = 0;
    while (block != 0) {
        uint16_t ugen;
        uint32_t rw;
        int rc;
        uint32_t sz;
        /* fetch current ugen address */
        ugen = 0;
        rc = ugen_block_get(mem, block, pos, &ugen);
        if (rc) return 3;
        /* set up rw function args */
        rw = (ugen << 16) | (mem[ugen + 1] & 0xFFFF);
        rc = sonilo_set(ctx->s, rw);
        if (rc) return 4;
        /* call direct */
        rc = sonilo_call_direct(ctx->s);
        if (rc) return 5;
        /* check rc flags */
        rc = sonilo_get(ctx->s, &rw);
        if (rc || rw) return 6;
        /* update pointers */
        pos++;
        if (pos >= UGEN_BLOCK_MAX) {
            block = next;
            pos = 0;
            rc = ugen_block_next(mem, block, &next);
        }

        sz = mem[block] & 0xFFFF;
        if (pos >= sz) break;
    }

    return 0;
}

int sonilo_mkugen(sonilo_ctx *ctx, const char *ugen)
{
    int rc;
    uint32_t *mem;
    uint16_t tail, uaddr;
    uint32_t val;

    /* push symbol */
    rc = sonilo_symbol(ctx, ugen);
    if (rc) return 1;

    /* ugen create */
    rc = sonilo_ugen_create(ctx);
    if (rc) return 2;

    /* pop address */
    val = 0;
    rc = sonilo_pop(ctx, &val);
    if (rc) return 3;
    uaddr = val;

    mem = ctx->s->mem;
    /* append to block tail */
    rc = context_ublock_tail_get(mem, ctx->context, &tail);
    if (rc) return 6;
   
    val = uaddr;
    rc = sonilo_push(ctx, val);
    if (rc) return 7;

    val = tail;
    rc = sonilo_push(ctx, val);
    if (rc) return 4;
    rc = ugen_block_append(mem, ctx->context);

    if (rc) return 5;
    /* update tail.
     * append pushes current tail address onto stack
     * pop it off the stack and store it in the context
     */
    rc = sonilo_pop(ctx, &val);
    if (rc) return 6;
    tail = val;
    rc = context_ublock_tail_set(mem, ctx->context, tail);
    if (rc) return 7;

    return 0;
}

int sonilo_last_ugen(sonilo_ctx *ctx, uint16_t *last)
{
    uint16_t tail;
    int nelem;
    int rc;
    uint32_t *mem;
    /* get tail */
    mem = ctx->s->mem;
    tail = 0;
    rc = context_ublock_tail_get(mem, ctx->context, &tail);
    if (rc) return 1;
    /* get number of elments */
    nelem = mem[tail] & 0xFFFF;
    /* if zero elements, return empty */
    if (nelem <= 0) return 2;
    if (last == NULL) return 3;
    /* retrieve address based on element number */
    rc = ugen_block_get(mem, tail, nelem - 1, last);
    if (rc) return 4;
    return 0;
}

int sonilo_create(sonilo **ps)
{
    int rc;
    sonilo *s;
    uint32_t *universe;
    uint32_t *blk;

    if (ps == NULL) return -1;
    universe = malloc(universe_size());
    s = malloc(sizeof(sonilo));

    s->universe = universe;

    blk = NULL;
    universe_get(universe, 0, &blk);
    s->vm = (sonilo_vm *)blk;
    rc = sonilo_vm_init(s->vm);
    if (rc) return rc;
    sonilo_host_init(&s->host, s->vm);
    
    universe_get(universe,
        WM_LOCATION,
        &blk);
    s->wm = (word_machine *)blk;
    word_machine_init(s->wm, 0);


    rc = sonilo_init(s);
    if (rc) return rc;
    *ps = s;
    return 0;
}

void sonilo_destroy(sonilo *s)
{
    uint32_t *u;
    u = s->universe;
    free(u);
    free(s);
    s = NULL;
}

uint32_t sonilo_vm_rw_get(sonilo_vm *vm)
{
    return vm->rw;
}

void sonilo_vm_rw_set(sonilo_vm *vm, uint32_t rw)
{
    vm->rw = rw;
}

uint32_t* sonilo_vm_rw_ptr(sonilo_vm *vm)
{
    return &vm->rw;
}

uint32_t sonilo_vm_err_get(sonilo_vm *vm)
{
    return vm->err;
}

void sonilo_vm_err_set(sonilo_vm *vm, uint32_t err)
{
    vm->err = err;
}

uint32_t* sonilo_vm_err_ptr(sonilo_vm *vm)
{
    return &vm->err;
}

uint32_t* sonilo_vm_mem(sonilo_vm *vm)
{
    return vm->mem;
}

uint32_t sonilo_vm_cursor_get(sonilo_vm *vm)
{
    return vm->cursor;
}

void sonilo_vm_cursor_set(sonilo_vm *vm, uint16_t cur)
{
    vm->cursor = cur;
}

void sonilo_vm_cursor_swap(sonilo_vm *vm)
{
    uint16_t tmp;
    tmp = vm->cursor;
    vm->cursor = vm->alt;
    vm->alt = tmp;
    vm->swap ^= 1;
}

void sonilo_vm_cursor_select(sonilo_vm *vm, int which)
{
    if (which) {
        /* want: cursor 1 */
        /* swap if unswapped to make cursor 1 active */
        if (!vm->swap) sonilo_vm_cursor_swap(vm);
    } else {
        /* want: cursor 0 */
        /* if swapped, swap to get cursor 0 active */
        if (vm->swap) sonilo_vm_cursor_swap(vm);
    }
}

uint16_t* sonilo_vm_cursor_ptr(sonilo_vm *vm)
{
    return &vm->cursor;
}

void sonilo_vm_read(sonilo_vm *vm)
{
    vm->rw = vm->mem[vm->cursor];
}

void sonilo_vm_write(sonilo_vm *vm)
{
    vm->mem[vm->cursor] = vm->rw;
}

int sonilo_vm_char(sonilo_vm *vm, char c)
{
    int b;
    uint32_t rw;
    rw = sonilo_vm_rw_get(vm);
    if (c == '\'') {
        /* shift 1-bit. assuming 3-characters, will make
         * it align to 16 bits */
        rw <<= 1;
        sonilo_vm_rw_set(vm, rw);
        return 0;
    }
    b = instr_char_sym(c);
    if (b < 0) return 1;
    rw <<= 5;
    rw |= b;
    sonilo_vm_rw_set(vm, rw);
    return 1;
}

size_t sonilo_host_sizeof(void)
{
    return sizeof(sonilo_host);
}

int sonilo_host_init(sonilo_host *host, sonilo_vm *vm)
{
    uint32_t i;
    /* set up map interface */
    for (i = 0; i < MAX_INSTR; i++) {
        host->func[i] = NULL;
    }
    host->map.key = vm->key;
    host->map.func = host->func;
    host->map.nent = &vm->nentries;
    return 0;
}

int sonilo_host_cfunc(sonilo_host *host, uint16_t key, instr_func func)
{
    instr_map_set(&host->map, key, func);
    return 0;
}

int sonilo_host_ex(sonilo_host *host, uint32_t *mem, uint32_t i, uint32_t *rw)
{
    return instr_ex(mem, &host->map, i, rw);
}

int sonilo_host_block(sonilo_host *host, uint32_t *mem, uint16_t p, uint32_t *rw)
{
    return instr_block(mem, &host->map, p, rw);
}

uint32_t sonilo_vm_blocklist(sonilo_vm *vm)
{
    return vm->blocklist;
}

int sonilo_send(sonilo *s, char c)
{
    return word_machine_send(s->wm, c);
}

int sonilo_sendw(sonilo *s, uint32_t w)
{
    /* TODO: implement */
    /* little endian? */
    return 1;
}

int sonilo_begin(sonilo *s)
{
    return word_machine_begin(s->wm);
}

/* end: end message and evaluate */
int sonilo_end(sonilo *s)
{
    /* TODO: implement */
    /* TODO: parse */
    /* TODO: clear buffer */
    return 1;
}

int sonilo_upush(sonilo *s)
{
    uint16_t src, dst, sz;
    uint32_t w;
    int rc;
    uint16_t stk;
    uint32_t *mem;

    stk = sonilo_vm_cursor_get(s->vm);
    mem = sonilo_vm_mem(s->vm);

    /* stack args: dst src sz */
    w = 0;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 1;
    sz = w & 0xFFFF;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 2;
    src = w & 0xFFFF;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 3;
    dst = w & 0xFFFF;

    rc = universe_push(s->universe, dst, src, sz);
    if (rc) return rc + 3;

    return 0;
}

int sonilo_upull(sonilo *s)
{
    uint16_t dst, src, sz;
    uint32_t w;
    int rc;
    uint16_t stk;
    uint32_t *mem;

    stk = sonilo_vm_cursor_get(s->vm);
    mem = sonilo_vm_mem(s->vm);

    /* stack args: dst src sz */
    w = 0;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 1;
    sz = w & 0xFFFF;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 2;
    src = w & 0xFFFF;

    rc = barray_pop(mem, stk, &w);
    if (rc) return 3;
    dst = w & 0xFFFF;

    rc = universe_pull(s->universe, dst, src, sz);
    if (rc) return rc + 3;
    return 1;
}

sonilo_vm* sonilo_get_vm(sonilo *s)
{
    return s->vm;
}

sonilo_host* sonilo_get_host(sonilo *s)
{
    return &s->host;
}
