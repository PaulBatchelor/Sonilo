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

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, 0,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static float powerwave(float x)
{
    float y;

    y = 0;

    if (x < 0.25) {
        /* P1 */
        x *= 4.0;
        x = (1 - x);
        y = -x*x + 1;
    } else if (x < 0.5) {
        /* P2 */
        x -= 0.25;
        x *= 4.0;
        y = -(x*x) + 1;
    } else if (x < 0.75) {
        x -= 0.5;
        x *= 4;
        x = (1 - x);
        y = x*x - 1;
    } else {
        /* P4 */
        x -= 0.75;
        x *= 4;
        y = x*x - 1;
    }

    return y;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port in, out;
    int n;

    ports = ugen_ports(mem, ugen);
    in = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i,  o;
        i = sonilo_port_read(&in, n);
        o = powerwave(i);
        sonilo_port_write(&out, n, o);
    }

    return 0;
}

int ugen_powerwave(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("PWV");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
