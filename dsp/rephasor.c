#include <math.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct sk_rephasor sk_rephasor;

struct sk_rephasor {
    float pr;
    float pc[2];
    float pe[2];
    float c;
    float s;
    float si;
    float ir;
    float ic;

    /* TODO: some place to cache num/den */
    /* TODO: some place to store IB/ITER addresses */
    /* TODO: some place to store mode */
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
    if (scale != rp->s) {
        rp->s = scale;
        rp->si = 1.0 / scale;
    }
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

    if (rp->c > 2.0 || rp->c < 0.5) rp->c = 1.0;

    out = pr;

    /* update state */

    rp->pr = pr;

    rp->pc[1] = rp->pc[0];
    rp->pc[0] = pc;

    rp->pe[1] = rp->pe[0];
    rp->pe[0] = ext;

    return out;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    sk_rephasor *rphs;

    /* TODO: get upper bits */
    /* TODO: split this into two different functions
     * based on upper bit configuration? */

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
    /* TODO: only create 1 iport if using iterator block,
     * get addresses from stack */
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
    /* TODO: initialize num/den value peaking at iterator */

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    sk_rephasor *rphs;
    uint32_t *ports;
    sonilo_port p_in, p_num, p_den, p_out;
    int n;

    /* TODO: set this top level function as a router based on mode */

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
