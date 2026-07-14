#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#define PID2 1.57079633

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
        5, 0,
        &ugen);
    if (rc) return 2;

    /* ports */
    for (i = 3; i >= 0; i--) {
        rc = ugen_iport(mem, ctx, ugen, i);
        if (rc) return 3;
    }

    rc = ugen_oport(mem, ctx, ugen, 4);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static float bezit(float u, float x, float y, float z)
{
    /* (pi/2) * u * (1-u)**2 + 3*u**2 - 2*u**3 */
    float f, o;
    f = (1.0 - u);
    o = (PID2*f*f*u)*x;
    f = u*u;
    o += (3.0*f)*y;
    f *= u;
    o += (-2*f)*z;
    return o;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port p_in, p_out, p_x, p_y, p_z;
    int n;

    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_x = sonilo_port_from_word(mem, ports[1]);
    p_y = sonilo_port_from_word(mem, ports[2]);
    p_z = sonilo_port_from_word(mem, ports[3]);
    p_out = sonilo_port_from_word(mem, ports[4]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i,  o, x, y, z;
        o = 0;
        i = sonilo_port_read(&p_in, n);
        x = sonilo_port_read(&p_x, n);
        y = sonilo_port_read(&p_y, n);
        z = sonilo_port_read(&p_z, n);
        if (i < 0.25) {
            o = bezit(4*i, x, y, z);
        } else if (i < 0.5) {
            o = bezit(2.0 - 4.0*i, x, y, z);
        } else if (i < 0.75) {
            o = -bezit(4.0*i - 2, x, y, z);
        } else {
            o = -bezit(4.0 - 4.0*i, x, y, z);
        }
        sonilo_port_write(&p_out, n, o);
    }

    return 0;
}

int ugen_bez(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("BEZ");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
