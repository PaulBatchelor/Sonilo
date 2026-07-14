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

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port p_in, p_mn, p_mx, p_out;
    int n;

    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_mn = sonilo_port_from_word(mem, ports[1]);
    p_mx = sonilo_port_from_word(mem, ports[2]);
    p_out = sonilo_port_from_word(mem, ports[3]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, mn, mx, out;

        in = sonilo_port_read(&p_in, n);
        mn = sonilo_port_read(&p_mn, n);
        mx = sonilo_port_read(&p_mx, n);

        out = (mx - mn)*in + mn;
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}

int ugen_scale(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SCL");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
