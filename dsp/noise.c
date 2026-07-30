#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"

#define LCG_MAX 2147483648

typedef struct dsp_noise {
    uint32_t rng;
} dsp_noise;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_noise *ns;
    uint32_t seed;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        1, sizeof(dsp_noise) >> 2,
        &ugen);
    if (rc) return 2;

    rc = ugen_oport(mem, ctx, ugen, 0);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    ns = (dsp_noise *)ugen_state(mem, ugen);
    if (ns == NULL) return 5;

    /* pop seed for RNG off of stack */
    rc = barray_pop(mem, stk, &seed);
    if (rc) return 7;

    ns->rng = seed;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t lcg(uint32_t rng)
{
    return (1103515245 * rng + 12345) % LCG_MAX;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_noise *ns;
    uint32_t *ports;
    sonilo_port p_out;
    int n;
    uint32_t rng;

    ns = (dsp_noise *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_out = sonilo_port_from_word(mem, ports[0]);

    rng = ns->rng;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float o;
        o = rng * (2.0 / LCG_MAX);
        o -= 1.0;
        rng = lcg(rng);
        sonilo_port_write(&p_out, n, o);
    }

    ns->rng = rng;

    return 0;
}

int ugen_noise(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("WNZ");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
