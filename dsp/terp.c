#include <math.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "iter.h"
#include "array.h"
#include "gv.h"

typedef struct dsp_terp dsp_terp;


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


static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    /* TODO: split into modes. one for indexed mode. one for regular mode */
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

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    /* TODO: make two render modes. one for regular, one
     * for index */
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
            printf("A: %g, B: %g\n", trp->A, trp->B);
        }

        out = ((1.0 - in)*trp->A) + in*trp->B;

        sonilo_port_write(&p_out, n, out);
    }
    return 0;
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
