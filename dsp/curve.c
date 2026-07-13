#include <math.h>
#include <stdint.h>
#include "sonilo.h"
#include "mem.h"
#include "context.h"
#include "ugen.h"
#include "iter.h"
#include "array.h"

#define GV_CURVE(X) ((X >> 16) & 0xFF)

typedef struct dsp_curve dsp_curve;

enum {
    /* curve as a regular signal processor */
    MODE_SIGNAL,
    /* curve as an iterator */
    MODE_ITER
};

/* curve types */

/* linear (pass-thru) */
#define CURVE_LIN 0

/* step (returns 0) */
#define CURVE_STEP 1

/* gliss 0-3 (curves at end of line) */
#define CURVE_GLISS_0 2
#define CURVE_GLISS_1 3
#define CURVE_GLISS_2 4
#define CURVE_GLISS_3 5

/* quadratic concave down/up */
#define CURVE_QUAD_DOWN 6
#define CURVE_QUAD_UP 7

/* tri: go from 0-1 to 0-1-0 */
#define CURVE_TRI 8

/* smoothstep function */
#define CURVE_SMOOTHSTEP 9

struct dsp_curve {
    /* iterator block address */
    uint16_t ib;

    /* ugen mode */
    uint16_t mode;
};

static uint32_t init_sig(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd;
    dsp_curve *crv;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        3, sizeof(dsp_curve) >> 2,
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
    crv = (dsp_curve *)ugen_state(mem, ugen);
    if (crv == NULL) return 5;

    crv->mode = MODE_SIGNAL;
    crv->ib = 0;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}


static uint32_t init_iter(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk, ugen;
    uint32_t cmd, x;
    dsp_curve *crv;

    /* command */
    stk = CTX_STACK(mem, ctx);
    cmd = 0;
    rc = barray_pop(mem, stk, &cmd);
    if (rc) return 1;

    /* intialize ugen */
    rc = ugen_create(mem,
        ctx,
        (uint16_t) cmd,
        2, sizeof(dsp_curve) >> 2,
        &ugen);
    if (rc) return 2;

    /* ports */
    rc = ugen_iport(mem, ctx, ugen, 0);
    if (rc) return 3;
    rc = ugen_oport(mem, ctx, ugen, 1);
    if (rc) return 4;

    context_pstack_sweep(mem, ctx);

    /* state */
    crv = (dsp_curve *)ugen_state(mem, ugen);
    if (crv == NULL) return 5;

    crv->mode = MODE_ITER;

    /* pop itblock off stack */

    rc = barray_pop(mem, stk, &x);
    if (rc) return 7;
    crv->ib = x;

    /* push ugen address */
    rc = barray_append(mem, stk, ugen);
    if (rc) return 6;

    return 0;
}

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    uint16_t mode;

    /* get upper bits */

    mode = mem[ctx + SLOT_UGEN_BITS] >> 16;

    if (mode == 1) return init_iter(mem, ctx);

    return init_sig(mem, ctx);
}

static float gliss_it(float phs, float pos)
{
    float a;

    if (phs < pos) {
        a = 0.0;
    } else {
        a = phs - pos;
        if (a < 0.0) {
            a = 0.0;
        }
        a /= 1.0 - pos;
        a = a * a * a;
    }
    return a;
}

static float curve_tick(float x, uint8_t type)
{
    switch (type) {
        case CURVE_LIN:
            return x;
        case CURVE_STEP:
            return 0;
        case CURVE_GLISS_0:
            return gliss_it(x, 0.9);
        case CURVE_GLISS_1:
            return gliss_it(x, 0.85);
        case CURVE_GLISS_2:
            return gliss_it(x, 0.75);
        case CURVE_GLISS_3:
            return gliss_it(x, 0.5);
        case CURVE_QUAD_DOWN:
            x = 1 - x;
            return -(x * x) + 1;
        case CURVE_QUAD_UP:
            return x * x;
        case CURVE_TRI:
            if (x > 0.5) return 1.0 - ((x - 0.5) * 2.0);
            else return 2.0 * x;
        case CURVE_SMOOTHSTEP:
            return 3*x*x - 2*x*x*x;
    }

    return x;
}

static uint32_t render_sig(uint32_t *mem, uint16_t ugen)
{
    uint32_t *ports;
    sonilo_port p_in, p_type, p_out;
    int n;

    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_type = sonilo_port_from_word(mem, ports[1]);
    p_out = sonilo_port_from_word(mem, ports[2]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, type, out;
        in = sonilo_port_read(&p_in, n);
        type = sonilo_port_read(&p_type, n);
        out = curve_tick(in, (uint8_t) type);
        sonilo_port_write(&p_out, n, out);
    }

    return 0;
}

static uint32_t render_iter(uint32_t *mem, uint16_t ugen)
{
    dsp_curve *crv;
    uint32_t *ports;
    sonilo_port p_in, p_out;
    int n;

    crv = (dsp_curve *)ugen_state(mem, ugen);
    ports = ugen_ports(mem, ugen);
    p_in = sonilo_port_from_word(mem, ports[0]);
    p_out = sonilo_port_from_word(mem, ports[1]);

    for (n = 0; n < UGEN_BLKSZ; n++) {
        float in, out;
        uint8_t type;
        uint32_t slice;

        in = sonilo_port_read(&p_in, n);

        slice = 0;
        iter_block_slice(mem, crv->ib, n, &slice);
        type = GV_CURVE(array_value(mem, slice));

        out = curve_tick(in, type);

        sonilo_port_write(&p_out, n, out);
    }
    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t ugen)
{
    dsp_curve *crv;

    crv = (dsp_curve *)ugen_state(mem, ugen);

    switch (crv->mode) {
        case MODE_SIGNAL:
            return render_sig(mem, ugen);
        case MODE_ITER:
            return render_iter(mem, ugen);
    }
    return 1;
}

int ugen_curve(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("CRV");
    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;
    return 0;
}
