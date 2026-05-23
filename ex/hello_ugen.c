#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.1415926535
#endif

#include "sonilo.h"

typedef struct {
    float phs;
} sine_data;

static uint32_t render(uint32_t *mem, uint16_t p)
{
    sonilo_ugen_data ugen;
    sonilo_port freq, amp, out;
    sine_data *state;
    int n;
    uint32_t sr;
    float phs;

    /* convert memory address to C struct wrapper */
    sonilo_ugen_get(mem, &ugen, p);

    /* get ports */
    freq = sonilo_port_from_word(mem, ugen.ports[0]);
    amp = sonilo_port_from_word(mem, ugen.ports[1]);
    out = sonilo_port_from_word(mem, ugen.ports[2]);

    /* ugen state */
    state = (sine_data *)ugen.state;

    /* global samplerate */
    sr = sonilo_srate(mem);
    phs = state->phs;

    for (n = 0; n < 64; n++) {
        float f, a, o;
        f = sonilo_port_read(&freq, n);
        a = sonilo_port_read(&amp, n);
        o = a * sin(2.0 * M_PI * phs);
        phs += f * (1.0 / sr);
        if (phs > 1) phs -= 1.0;

        sonilo_port_write(&out, n, o);
    }

    state->phs = phs;

    return 0;
}

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    sonilo_ugen ugen;
    float *out;
    uint32_t i;
    uint16_t ukey;
    int rc;
    FILE *fp;
    sine_data *state;

    fp = NULL;

    s = NULL;
    rc = sonilo_create(&s);
    sonilo_ctx_init(&ctx, s);

    /* push amp/freq constants */
    rc = sonilo_constant(&ctx, 440);
    if (rc) goto clean;
    rc = sonilo_constant(&ctx, 0.5);
    if (rc) goto clean;

    /* bind DSP command to sonilo */
    ukey = sonilo_key("ZZZ");
    rc = sonilo_command(s, ukey, render);
    if (rc) {
        goto clean;
    }

    /* allocate/initialize ugen */
    rc = sonilo_ugen_init(&ctx, &ugen, ukey, 3, 4);
    if (rc) {
        goto clean;
    }

    /* set up ports: two inputs, one output */
    rc = sonilo_iport(&ugen, 1);
    if (rc) goto clean;
    rc = sonilo_iport(&ugen, 0);
    if (rc) goto clean;
    rc = sonilo_oport(&ugen, 2);
    if (rc) goto clean;

    /* perform GC sweep to return any freed buffers */
    sonilo_flush(&ctx);

    /* initialize ugen internal state */
    state = (sine_data *)ugen.data.state;
    state->phs = 0;

    /* get output of ugen */
    rc = sonilo_ugen_block(sonilo_mem(ctx.s), ugen.data.top, 2, &out);

    if (rc) {
        /* not a block, for some reason */
        printf("not a block, for some reason");
        goto clean;
    }

    fp = fopen("out.raw", "wb");
    for (i = 0; i < 3445; i++) {
        sonilo_ugen_compute(&ugen);
        /* write contents of output to disk */
        fwrite(out, sizeof(float), 64, fp);
    }

    /* cleanup */
clean:
    sonilo_ctx_destroy(&ctx);
    sonilo_destroy(s);
    if (fp != NULL) fclose(fp);
    return 0;
}
