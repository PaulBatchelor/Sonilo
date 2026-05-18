#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "iter.h"

typedef struct dsp_seq {
    uint16_t iter;
    float val;
} dsp_seq;

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen, iter;
    uint32_t cmd, val;
    dsp_seq *seq;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_seq) >> 2,
        &ugen);
    if (rc) return 2;

    /* get iterator address from bstack */
    val = 0;
    rc = barray_pop(mem, stk, &val);
    if (rc) return 7;
    iter = val & 0xFFFF;


    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    seq = (dsp_seq *)ugen_state(mem, ugen);
    if (seq == NULL) return 5;
    seq->iter = iter;
    seq->val = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_seq *seq;
    uint32_t *ports;
    sonilo_port trig, out;
    int n;
    float val;

    seq = (dsp_seq *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    trig = sonilo_port_from_word(mem, ports[0]);
    out = sonilo_port_from_word(mem, ports[1]);
    val = seq->val;

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float t;
        t = sonilo_port_read(&trig, n);
        if (t) {
            val = iter_real(mem, seq->iter);
        }
        sonilo_port_write(&out, n, val);
    }

    seq->val = val;

    return 0;
}

int ugen_seq(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SEQ");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
