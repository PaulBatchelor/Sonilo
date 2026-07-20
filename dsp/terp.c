#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "iter.h"
#include "array.h"
#include "gv.h"

typedef struct dsp_terp dsp_terp;
enum {
    REGULAR,
    INDEXED
};

struct dsp_terp {
    /* mode, iterator block (1 word) */
    uint16_t ib, mode;
    /* A/B values for interpolation A -> B (2 words) */
    float A, B;
    /* lookup table (halfword) */
    uint16_t lookup;
    /* padding to make the struct evenly 4 words */
    uint16_t padding;
};


static uint32_t init0(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd, x;
    dsp_terp *trp;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_terp) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    trp = (dsp_terp *)ugen_state(mem, ugen);
    if (trp == NULL) return 5;

    /* pop itblock off stack */

    rc = barray_pop(mem, stk, &x);
    if (rc) return 7;
    trp->ib = x;
    trp->mode = REGULAR;
    trp->lookup = trp->padding = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t init_indexed(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd, x;
    dsp_terp *trp;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_terp) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    trp = (dsp_terp *)ugen_state(mem, ugen);
    if (trp == NULL) return 5;

    /* pop itblock off stack */

    rc = barray_pop(mem, stk, &x);
    if (rc) return 7;
    trp->ib = x;

    /* pop lookup index off stack */
    
    rc = barray_pop(mem, stk, &x);
    if (rc) return 8;
    trp->lookup = x;
    trp->mode = INDEXED;
    trp->padding = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    uint16_t mode;
    mode = mem[ctx + SLOT_UGEN_BITS] >> 16;

    if (mode == 1) return init_indexed(mem, ctx);

    return init0(mem, ctx);
}

static uint32_t render0(uint32_t *mem, uint16_t ugen)
{
    dsp_terp *trp;
    uint32_t *ports;
    sonilo_port p_in, p_out;
    int n;

    trp = (dsp_terp *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, out;
        uint32_t slice;

        in = sonilo_port_read(&p_in, n);

        slice = 0;
        iter_block_trig(mem, trp->ib, n, &slice);

        if (slice) {
            uint16_t it;
            it = 0;
            iter_block_iter(mem, trp->ib, &it);
            iter_block_slice(mem, trp->ib, n, &slice);
            /* trp->A = GV_VAL(array_value(mem, slice)); */
            trp->A = iter_real_slice(mem, it, slice);
            iter_block_next(mem, trp->ib, n, &slice);
            /* trp->B = GV_VAL(array_value(mem, slice)); */
            trp->B = iter_real_slice(mem, it, slice);
        }

        out = ((1.0 - in)*trp->A) + in*trp->B;

        sonilo_port_write(&p_out, n, out);
    }
    return 0;
}

static uint32_t render_indexed(uint32_t *mem, uint16_t ugen)
{
    dsp_terp *trp;
    uint32_t *ports;
    sonilo_port p_in, p_out;
    int n;

    trp = (dsp_terp *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, out;
        uint32_t slice;

        in = sonilo_port_read(&p_in, n);

        slice = 0;
        iter_block_trig(mem, trp->ib, n, &slice);

        if (slice) {
            uint16_t it;
            uint16_t a;
            uint8_t type;
            uint32_t idx[2];
            uint32_t bslice;

            it = 0;

            iter_block_iter(mem, trp->ib, &it);

            /* extract lookup index */
            iter_block_slice(mem, trp->ib, n, &slice);
            bslice = 0;
            iter_block_next(mem, trp->ib, n, &bslice);
            idx[0] = array_value(mem, slice);
            idx[1] = array_value(mem, bslice);

            /* if main iterator is GV, extract value */
            a = iter_get_array(mem, it);
            type = 0;
            array_type_get(mem, a, &type);
            if (type == ARRAY_TYPE_GVERT) {
                idx[0] = GV_VAL(idx[0]);
                idx[1] = GV_VAL(idx[1]);
            }

            /* get slice/type from lookup */
            array_type_get(mem, trp->lookup, &type);
            array_read_direct(mem, trp->lookup, idx[0], &slice);
            array_read_direct(mem, trp->lookup, idx[1], &bslice);

            trp->A = array_real(mem, slice, type);
            trp->B = array_real(mem, bslice, type);
        }

        out = ((1.0 - in)*trp->A) + in*trp->B;

        sonilo_port_write(&p_out, n, out);
    }
    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_terp *trp;


    trp = (dsp_terp *)ugen_state(mem, ugen);

    switch (trp->mode) {
        case REGULAR:
            return render0(mem, ugen);
        case INDEXED:
            return render_indexed(mem, ugen);
    }

    return 1;
}

int ugen_terp(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("TRP");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;
    return 0;
}
