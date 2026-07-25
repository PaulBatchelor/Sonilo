#include <stdint.h>
#include <math.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_vib {
    float cents;
    float min;
    float max;
} dsp_vib;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_vib *vib;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(dsp_vib) >> 2,
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
    vib = (dsp_vib *)ugen_state(mem, ugen);
    if (vib == NULL) return 5;

    vib->cents = 0;
    vib->min = vib->max = 1;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_vib *vib;
    uint32_t *ports;
    sonilo_port p_cents, p_out, p_in;
    int n;

    vib = (dsp_vib *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_cents = sonilo_port_from_word(mem, ports[1]);
    p_out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i,  c, o;
        i = sonilo_port_read(&p_in, n);
        c = sonilo_port_read(&p_cents, n);

        if (c != vib->cents) {
            vib->cents = c;
            vib->min = pow(2.0, -c / 1200.0);
            vib->max = pow(2.0, c / 1200.0);
        }

        i += 1.0; i *= 0.5;
        o = vib->min + (vib->max - vib->min)*i;
        sonilo_port_write(&p_out, n, o);
    }

    return 0;
}

int ugen_vib(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("VIB");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
