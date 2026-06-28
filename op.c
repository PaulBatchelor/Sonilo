#include <stdio.h>
#include "sonilo.h"
#include "context.h"
#include "ugen.h"
#include "mem.h"
#include "ins.h"
#include "array.h"
#include "iter.h"

#if 0
/* populate a context struct from an existing context address */
static void hydrate_context(sonilo *s, sonilo_ctx *ctx, uint16_t ac)
{
    uint32_t *mem;
    sonilo_vm *vm;
    vm = sonilo_get_vm(s);
    mem = sonilo_vm_mem(vm);
    ctx->s = s;
    ctx->context = ac;
    ctx->allocator = CTX_ALLOC(mem, ac);
    ctx->pstack = CTX_PARAM_STACK(mem, ac);
}
#endif

static int a_array(sonilo_vm *vm, uint8_t data)
{
    int rc;
    uint32_t *mem;
    uint16_t stk;
    uint16_t ctx;

    rc = -1;
    mem = sonilo_vm_mem(vm);
    ctx = sonilo_vm_cursor_get(vm);
    stk = CTX_STACK(mem, ctx);
    switch (data) {
        case 0:
            rc = array_create(mem, stk);
            break;
        case 1:
            rc = array_write(mem, stk);
            break;
        default:
            rc = -1;
    }
    return rc;
}

static int a_iter(sonilo_vm *vm, uint8_t data)
{
    int rc;
    uint32_t *mem;
    uint16_t ctx;
    uint16_t iter, arr, stk;
    uint32_t val;

    rc = -1;
    ctx = sonilo_vm_cursor_get(vm);
    mem = sonilo_vm_mem(vm);
    iter = arr = 0;
    stk = CTX_STACK(mem, ctx);
    switch (data) {
        case 0:
            rc = barray_pop(mem, stk, &val);
            if (rc) { rc = 4; break; }
            arr = val;
            rc = iter_alloc(mem, ctx, &iter);
            if (rc) { rc = 1; break; }
            rc = iter_init(mem, iter);
            if (rc) { rc = 2; break; }
            rc = iter_array(mem, iter, arr);
            if (rc) { rc = 3; break; }
            rc = barray_append(mem, stk, iter);
            if (rc) { rc = 5; break; }
            break;
         default:
            rc = -1;
    }
    return rc;
}

static int last_ugen(sonilo_vm *vm, uint16_t ctx)
{
    uint16_t tail;
    int nelem;
    int rc;
    uint32_t *mem;
    uint16_t last;

    last = 0;
    /* get tail */
    mem = sonilo_vm_mem(vm);
    tail = 0;
    rc = context_ublock_tail_get(mem, ctx, &tail);
    if (rc) return 1;
    /* get number of elments */
    nelem = mem[tail] & 0xFFFF;
    /* if zero elements, return empty */
    if (nelem <= 0) return 2;
    /* retrieve address based on element number */
    rc = ugen_block_get(mem, tail, nelem - 1, &last);
    if (rc) return 4;

    sonilo_vm_rw_set(vm, last);
    return 0;
}

static int a_ugen(sonilo_vm *vm, uint32_t data)
{
    int rc;
    uint16_t ctx;
    rc = -1;

    ctx = sonilo_vm_cursor_get(vm);

    switch (data) {
        case 0:
            rc = last_ugen(vm, ctx);
            break;
        default:
            rc = -1;
            break;
    }
    return rc;
}

static int mkugen(sonilo *s, uint16_t ac, uint32_t key)
{
    int render;
    uint32_t rw;
    int rc;
    sonilo_host *host;
    uint16_t stk;
    uint32_t *mem;
    sonilo_vm *vm;

    host = sonilo_get_host(s);
    vm = sonilo_get_vm(s);
    mem = sonilo_vm_mem(vm);
    stk = CTX_STACK(mem, ac);

    /* look up entry for DSP callback (alt key) */
    render = sonilo_host_index(host, sonilo_alt(key));
    if (render < 0) return 6;

    /* push function index of render callback to stack */
    rc = barray_append(mem, stk, render);
    if (rc) return 7;

    /* pack bits: CMD | INSTR */
    rc = sonilo_set(s, (ac << 16) | (key & 0xFFFF));
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

static int b_ugen(sonilo *s, uint32_t data)
{
    sonilo_vm *vm;
    uint16_t ac;
    uint32_t *mem;
    uint16_t stk;
    uint32_t val;
    int rc;
    uint16_t tail, uaddr;

    vm = sonilo_get_vm(s);
    mem = sonilo_vm_mem(vm);
    ac = sonilo_vm_cursor_get(vm);

    /* push key onto stack */
    stk = CTX_STACK(mem, ac);
    /* ugen create */
    rc = mkugen(s, ac, data);
    if (rc) return 2;

    /* pop address */
    val = 0;
    rc = barray_pop(mem, stk, &val);
    if (rc) return 3;
    uaddr = val;

    /* append to block tail */
    rc = context_ublock_tail_get(mem, ac, &tail);
    if (rc) return 6;
   
    val = uaddr;
    rc = barray_append(mem, stk, val);
    if (rc) return 7;

    val = tail;
    rc = barray_append(mem, stk, val);
    if (rc) return 4;
    rc = ugen_block_append(mem, ac);

    if (rc) return 5;
    /* update tail.
     * append pushes current tail address onto stack
     * pop it off the stack and store it in the context
     */
    rc = barray_pop(mem, stk, &val);
    if (rc) return 6;
    tail = val;
    rc = context_ublock_tail_set(mem, ac, tail);
    if (rc) return 7;

    return 0;
}

static int b_const(sonilo_vm *vm, uint32_t data)
{
    uint16_t ctx;
    uint16_t pstack;
    uint32_t *mem;
    uint32_t w;
    int rc;

    ctx = sonilo_vm_cursor_get(vm);
    mem = sonilo_vm_mem(vm);
    pstack = CTX_PARAM_STACK(mem, ctx);

    /* convert constant data to port word */
    w = PORT_CONSTANT;
    w |= data << 2;

    /* push param onto stack */
    rc = pstack_push(mem, pstack, w);

    if (rc) {
        /* err 1 or 2: overflow or RC address not found for block */
        if (rc < 3) return rc;
        /* unknown error */
        else return 3;
    }

    return 0;
}

static int b_word(sonilo_vm *vm, uint32_t data)
{
    uint16_t ctx;
    uint32_t *mem;

    ctx = sonilo_vm_cursor_get(vm);
    mem = sonilo_vm_mem(vm);

    return barray_append(mem, CTX_STACK(mem, ctx), data);
}

static int a_word(sonilo_vm *vm, uint16_t data)
{
    uint16_t ctx;
    uint32_t *mem;
    uint32_t val;
    int rc;

    ctx = sonilo_vm_cursor_get(vm);
    mem = sonilo_vm_mem(vm);

    rc = -1;
    switch (data) {
        case 0: /* pop word, store in RW */
            val = 0;
            rc = barray_pop(mem, CTX_STACK(mem, ctx), &val);
            if (rc) { rc = 1; break; }
            sonilo_vm_rw_set(vm, val);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

static int a_ctx(sonilo *s, uint8_t data)
{
    sonilo_ctx ctx;
    int rc;

    rc = -1;
    switch (data) {
        case 0: /* create context */
            rc = sonilo_ctx_init(&ctx, s);
            if (rc) return 1;
            sonilo_set(s, ctx.context);
            break;
        case 1: { /* destroy context */
            uint16_t ac;
            ac = sonilo_vm_cursor_get(sonilo_get_vm(s));
            context_destroy(sonilo_mem(s), ac);
        }
        default:
            rc = -1;
            break;
    }

    return rc;
}

int a_tape(sonilo *s, uint8_t data)
{
    int rc;
    uint32_t rw;
    uint16_t msb, lsb;

    rc = -1;

    rw = 0;
    sonilo_get(s, &rw);
    msb = rw >> 16;
    lsb = rw & 0xFFFF;

    switch (data) {
        case 0: /* open track (LSB: track number) */
            rc = sonilo_tape_open(s, lsb);
            break;
        case 1: /* close track (LSB: track number) */
            rc = sonilo_tape_close(s, lsb);
            break;
        case 2: /* track bind (LSB: track number, MSB: sink */
            rc = sonilo_tape_bind(s, lsb, msb);
            break;
        default:
            rc = -1;
    }

    return rc;
}

int sonilo_op_a(sonilo *s, char type, uint8_t data)
{
    int rc;
    sonilo_vm *vm;

    vm = sonilo_get_vm(s);

    rc = 0;
    switch (type) {
        case '!': /* print rw */
            printf("%x\n", sonilo_vm_rw_get(vm));
            break;
        case 'a': /* array */
            rc = a_array(vm, data);
            break;
        case 'i': /* iterator */
            rc = a_iter(vm, data);
            break;
        case 'u': /* ugens */
            rc = a_ugen(vm, data);
            break;
        case 'C': /* create context */
            rc = a_ctx(s, data);
            break;
        case 'w': /* create context */
            rc = a_word(vm, data);
            break;
        case 't': /* tape */
            rc = a_tape(s, data);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

static int b_render(sonilo *s, uint32_t data)
{
    uint16_t ctx, nsecs;
    nsecs = data & 0xFFFF;
    ctx = sonilo_vm_cursor_get(sonilo_get_vm(s));

    return sonilo_render(s, ctx, nsecs);
}

int sonilo_op_b(sonilo *s, char type, uint32_t data)
{
    int rc;
    sonilo_vm *vm;
    vm = sonilo_get_vm(s);

    rc = 0;
    switch (type) {
        case '!':
            /* set rw */
            sonilo_vm_rw_set(vm, data);
            break;
        case 'u':
            rc = b_ugen(s, data);
            break;
        case 'w':
            rc = b_word(vm, data);
            break;
        case 'c':
            rc = b_const(vm, data);
            break;
        case 'r':
            rc = b_render(s, data);
            break;
        default:
            rc = 1;
            break;
    }

    return rc;
}
