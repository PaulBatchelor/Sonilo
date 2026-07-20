#include <math.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "iter.h"
#include "array.h"

#define GV_DUR(X) (X & 0xFFFF)
#define GV_DUR_NUM(X) (X >> 8)
#define GV_DUR_DEN(X) (X & 0xFF)

typedef struct sk_rephasor sk_rephasor;

enum {
    /* rephasor as a regular signal processor */
    MODE_SIGNAL,
    /* rephasor as an iterator */
    MODE_ITER
};

struct sk_rephasor {
    float pr;
    float pc[2];
    float pe[2];
    float c;
    float s;
    float si;
    float ir;
    float ic;

    /* IB/ITER addresses */
    uint32_t ibit;

    /* ugen mode */
    uint16_t mode;

    float prv;
};

static void sk_rephasor_init(sk_rephasor *rp)
{
    rp->pr = 0;
    rp->pc[0] = 0;
    rp->pc[1] = 0;
    rp->pe[0] = 0;
    rp->pe[1] = 0;
    rp->c = 1.0;
    rp->s = 1.0;
    rp->si = 1.0;

    rp->ir = 0.0;
    rp->ic = 0.0;
}

static void sk_rephasor_scale(sk_rephasor *rp, float scale)
{
    rp->s = scale;
    rp->si = 1.0 / scale;
}

/* implementation of a truncated phasor */

static float phasor(float phs, float inc)
{
    phs += inc;

    if (phs > 1.0) return 0;

    return phs;
}

static float sk_rephasor_tick(sk_rephasor *rp, float ext)
{
    float pr, pc;
    float out;

    /* delta function of \theta_e */
    if (ext > rp->pe[0]) {
        rp->ir = ext - rp->pe[0];
    }

    /* compute main rephasor \theta_r */
    pr = phasor(rp->pr, rp->s * rp->ir * rp->c);

    /* delta function of \theta_r */
    if (pr > rp->pr) {
        rp->ic = pr - rp->pr;
    }

    /* compute rephasor \theta_c */
    pc = phasor(rp->pc[0], rp->si * rp->ic);

    /* compute correction coefficient */
    if (rp->pc[1] != 0) {
        rp->c = rp->pe[1] / rp->pc[1];
    }

    /* if (rp->c > 2.0 || rp->c < 0.5) rp->c = 1.0; */
    if (rp->c > 2.0) rp->c = 2.0;
    if (rp->c < 0.5) rp->c = 0.5;

    out = pr;

    /* update state */

    rp->pr = pr;

    rp->pc[1] = rp->pc[0];
    rp->pc[0] = pc;

    rp->pe[1] = rp->pe[0];
    rp->pe[0] = ext;

    return out;
}

static uint32_t init_sig(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    sk_rephasor *rphs;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        4, sizeof(sk_rephasor) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 2);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 3);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    rphs = (sk_rephasor *)ugen_state(mem, ugen);
    if (rphs == NULL) return 5;

    sk_rephasor_init(rphs);
    rphs->mode = MODE_SIGNAL;
    rphs->prv = -1;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}


static uint32_t init_iter(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd, x, slice;
    sk_rephasor *rphs;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    /* NOTE: only 2 ports needed: 1 in, 1 out */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(sk_rephasor) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    rphs = (sk_rephasor *)ugen_state(mem, ugen);
    if (rphs == NULL) return 5;

    sk_rephasor_init(rphs);

    rphs->mode = MODE_ITER;

    /* pop iterator and itblock off stack */
    x = 0;

    /* itblock (ibit MSB) */
    rc = barray_pop(mem, stk, &x);
    if (rc) return 7;
    rphs->ibit = (x & 0xFFFF) << 16;

    /* iter (ibit LSB) */
    rc = barray_pop(mem, stk, &x);
    if (rc) return 8;
    rphs->ibit |= x & 0xFFFF;
    rphs->prv = -1;

    /* initialize num/den value peaking at iterator */

    slice = iter_get(mem, rphs->ibit & 0xFFFF);
    slice = GV_DUR(array_value(mem, slice));
    sk_rephasor_scale(rphs,
        (float)GV_DUR_DEN(slice) / (float)GV_DUR_NUM(slice)
    );

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    uint16_t mode;

    /* get upper bits */

    mode = mem[ctx + SLOT_UGEN_BITS] >> 16;

    if (mode == 1) return init_iter(mem, ctx);

    return init_sig(mem, ctx);
}

static uint32_t render_sig(uint32_t *mem, uint16_t ugen)
{
    sk_rephasor *rphs;
    uint32_t *ports;
    sonilo_port p_in, p_num, p_den, p_out;
    int n;

    rphs = (sk_rephasor *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_num = sonilo_port_from_word(mem, ports[1]);
    p_den = sonilo_port_from_word(mem, ports[2]);
    p_out = sonilo_port_from_word(mem, ports[3]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, num, den, out;
        in = sonilo_port_read(&p_in, n);
        num = sonilo_port_read(&p_num, n);
        den = sonilo_port_read(&p_den, n);
        if (den != 0) {
            /* use inverse because it is more musically intituive
             * for 1/2 to mean half a beat than 1/2 speed */
            sk_rephasor_scale(rphs, den / num);
        }
        out = sk_rephasor_tick(rphs, in);
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}


static uint32_t render_iter(uint32_t *mem, uint16_t ugen)
{
    sk_rephasor *rphs;
    uint32_t *ports;
    sonilo_port p_in, p_out;
    int n;

    rphs = (sk_rephasor *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, out, prv, trig;
        uint16_t ib, it;
        in = sonilo_port_read(&p_in, n);

        ib = rphs->ibit >> 16;
        it = rphs->ibit & 0xFFFF;

        prv = rphs->prv;
        out = sk_rephasor_tick(rphs, in);

        /* check if there is a new period, and run iterator */
        trig = (out < prv) || (prv < 0) ? 1.0 : 0.0;

        iter_block_tick(mem, ib, it, trig, n);

        /* update rephasor value on new period */
        if (trig > 0) {
            uint32_t slice, val;
            slice = 0;
            iter_block_slice(mem, ib, n, &slice);
            val = array_value(mem, slice);

            /* assume value is gesture vertex, extract duration */
            val = GV_DUR(val);
            sk_rephasor_scale(rphs,
                (float)GV_DUR_DEN(val) / (float)GV_DUR_NUM(val)
            );
        }

        sonilo_port_write(&p_out, n, out);
        rphs->prv = out;
    }

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    sk_rephasor *rphs;

    rphs = (sk_rephasor *)ugen_state(mem, ugen);

    switch (rphs->mode) {
        case MODE_SIGNAL:
            return render_sig(mem, ugen);
        case MODE_ITER:
            return render_iter(mem, ugen);
    }

    return 1;
}

int ugen_rephasor(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("RPH");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
