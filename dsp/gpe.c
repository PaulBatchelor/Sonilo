/* gpe: glottal pulse envelope */

#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_gpe {
    float lag;
    float size;
    float start;
    float phs;
} dsp_gpe;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_gpe *gpe;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(dsp_gpe) >> 2,
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
    gpe = (dsp_gpe *)ugen_state(mem, ugen);
    if (gpe == NULL) return 5;

    gpe->phs = -1;
    gpe->start = 0;
    gpe->lag = 0.07; /* 7% of period (max 15) */
    gpe->size = 0.6; /* 40-80 percent */

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

float gpe_tick(dsp_gpe *gpe, float phs)
{
    /* size must be non-zero */
    if (gpe->size == 0) return 0;

    /* remove start bias, determine bounds */
    phs -= gpe->start;
    if (phs < 0 || phs > gpe->size) {
        return 0;
    }

    /* normalize 0-1 */
    phs /= gpe->size;

    /* create triangle */

    if (phs < 0.5) phs *= 2;
    else phs = 1.0 - ((phs - 0.5) * 2.0);

    /* apply quadratic function */
    /* f(x) = -(1 - x)^2 + 1 */
    phs = 1 - phs;
    phs *= phs;
    phs = -phs;
    phs += 1;

    return phs;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_gpe *gpe;
    uint32_t *ports;
    sonilo_port p_in, p_shp, p_out;
    float phs;
    int n;

    gpe = (dsp_gpe *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_shp = sonilo_port_from_word(mem, ports[1]);
    p_out = sonilo_port_from_word(mem, ports[2]);

    phs = gpe->phs;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i, o, s;
        i = sonilo_port_read(&p_in, n);
        s = sonilo_port_read(&p_shp, n);
        if (phs < 0 || i < phs) {
            /* new period: calculate start */    
            float Rd, Ra, Rk, Rg;
            float Te, Tp;

            Rd = s;
            if (Rd < 0.5) Rd = 0.5;
            if (Rd > 2.7) Rd = 2.7;
            Ra = -0.01 + 0.048*Rd;
            Rk = 0.224 + 0.118*Rd;
            Rg = (Rk/4)*(0.5 + 1.2*Rk)/(0.11*Rd-Ra*(0.5+1.2*Rk));
            Tp = (float)1.0 / (2*Rg);
            Te = Tp + Tp*Rk;

            /* calculate envelope start from lag
             * and glottal closure (Te) (note that Te is normalized)
             * make sure to factor in delay to make it centered
             */
        
            gpe->start =
                (Te + gpe->lag) - 0.5*gpe->size;
        }
        o = gpe_tick(gpe, i);
        phs = i;
        sonilo_port_write(&p_out, n, o);
    }

    gpe->phs = phs;

    return 0;
}

int ugen_gpe(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("GPE");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
