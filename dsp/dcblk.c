#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

typedef struct dsp_dcblk {
    float x, y;
} dsp_dcblk;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_dcblk *dc;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_dcblk) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    dc = (dsp_dcblk *)ugen_state(mem, ugen);
    if (dc == NULL) return 5;

    dc->x = dc->y = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_dcblk *dc;
    uint32_t *ports;
    sonilo_port in, out;
    int n;

    dc = (dsp_dcblk *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    in = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float i,  o;
        i = sonilo_port_read(&in, n);
        dc->y = i - dc->x + 0.99*dc->y;
        dc->x = i;
        o = dc->y;
        sonilo_port_write(&out, n, o);
    }


    return 0;
}

int ugen_dcblk(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("DCB");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
