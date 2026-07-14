#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    int i;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        4, 0,
        &ugen);
    if (rc) return 2;

    /* ports */
    for (i = 2; i >= 0; i--) {
        rc = ugen_iport(mem, ctx, ugen, i);
        if (rc) return 3;
    }
    rc = ugen_oport(mem, ctx, ugen, 3);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static float plin(float in, float piv)
{
    if (in <= piv) {
        return (0.5 * in) / piv;
    }

    return 0.5 + (0.5*(in - piv) / (1.0 - piv));
}

static float smooth_pivot(float in, float piv, float smo)
{
    float wc;
    float x0, x1;
    float y0, y1;
    float m0, m1;
    float t;
    float h00, h01, h10, h11;
    float s;

    if (piv == 0.0 || piv == 1.0) return in;

    /* wc = max(0, min(w, p, 1 - p)) */
    wc = smo;
    if (piv < wc) wc = piv;
    if ((1 - piv) < wc) wc = 1 - piv;
    if (0 > wc) wc = 0;

    /* if wc < eps: return plin(x, p) */
    if (wc < 0.000001) return plin(in, piv);

    /* x0 = p - wc, x1 = p + wc */
    x0 = piv - wc;
    x1 = piv + wc;

    if (in <= x0 || in >= x1) return plin(in, piv);

    y0 = plin(x0, piv);
    m0 = 0.5 / piv;

    y1 = plin(x1, piv);
    m1 = 0.5 / (1 - piv);

    t = (in - x0) / (2.0 * wc);

    /* hermite cubic basis */

    /* h00 = 2t^3 - 3t^2 + 1*/
    h00 = 2*t*t*t - 3*t*t + 1;
    /* h10 = t^3 - 2t^2 + t */
    h10 = t*t*t - 2*t*t + t;
    /* h01 = -2t^3 + 3t^2 */
    h01 = -2*t*t*t + 3*t*t;
    /* h11 = t^3 - t^2 */
    h11 = t*t*t - t*t;

    s = 2.0 * wc;

    return h00*y0 + h10*s*m0 + h01*y1 + h11*s*m1;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port p_in, p_piv, p_smo, p_out;
    int n;

    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_piv = sonilo_port_from_word(mem, ports[1]);
    p_smo = sonilo_port_from_word(mem, ports[2]);
    p_out = sonilo_port_from_word(mem, ports[3]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, piv, smo, out;

        in = sonilo_port_read(&p_in, n);
        piv = sonilo_port_read(&p_piv, n);
        smo = sonilo_port_read(&p_smo, n);

        out = smooth_pivot(in, piv, smo);
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}

int ugen_warp(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("WRP");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
