#include <stdint.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct dsp_balance {
    float asig, csig, ihp;
    float c1, c2, prvq, prvr, prva;
} dsp_balance;

static void bal_init(dsp_balance *p, uint32_t sr)
{
    float b;

    p->ihp = 10;
    b = 2.0 - cos((float)(p->ihp * (2.0 * M_PI / sr)));
    p->c2 = b - sqrt(b*b - 1.0);
    p->c1 = 1.0 - p->c2;
    p->prvq = p->prvr = p->prva = 0.0;
}

float bal_tick(dsp_balance *p, float sig, float comp)
{
    float q, r, a, diff;
    float as, cs;
    float c1, c2;
    float out;
    out = 0;

    as = sig;
    cs = comp;

    c1 = p->c1, c2 = p->c2;
    q = p->prvq;
    r = p->prvr;

    q = c1 * as * as + c2 * q;
    r = c1 * cs * cs + c2 * r;

    p->prvq = q;
    p->prvr = r;

    if (q != 0.0) {
        a = sqrt(r/q);
    } else {
        a = sqrt(r);
    }

    if ((diff = a - p->prva) != 0.0) {
        out = sig * p->prva;
        p->prva = a;
    } else {
        out = sig * a;
    }

    return out;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_balance *bal;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(dsp_balance) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 1);
    if (rc) return 3;
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 2);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    bal = (dsp_balance *)ugen_state(mem, ugen);
    if (bal == NULL) return 5;

    bal_init(bal, sonilo_srate(mem));

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_balance *bal;
    uint32_t *ports;
    sonilo_port p_cmp, p_sig, p_out;
    int n;

    bal = (dsp_balance *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_cmp  = sonilo_port_from_word(mem, ports[0]);
    p_sig = sonilo_port_from_word(mem, ports[1]);
    p_out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float s, c, o;
        c = sonilo_port_read(&p_cmp, n);
        s = sonilo_port_read(&p_sig, n);
        o = bal_tick(bal, s, c);
        sonilo_port_write(&p_out, n, o);
    }

    return 0;
}

int ugen_balance(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("BAL");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
