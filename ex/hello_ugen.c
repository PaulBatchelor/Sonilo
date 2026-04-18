#include <stdio.h>
#include <stdlib.h>
#include "sonilo.h"

typedef struct {
    float phs;
} sine_data;

static uint32_t render(uint32_t *mem, uint16_t p)
{
    int rc;
    sonilo_ugen ugen;
    sonilo_port freq, amp, out;
    sine_data *state;
    int n;
    uint32_t sr;

    /* convert memory address to C struct wrapper */
    sonilo_ugen_get(mem, &ugen, p);

    /* get ports */
    sonilo_ugen_port(mem, ugen.ports[0], &freq);
    sonilo_ugen_port(mem, ugen.ports[1], &amp);
    sonilo_ugen_port(mem, ugen.ports[2], &out);

    /* ugen state */
    state = (sine_data *)ugen.state;

    /* global samplerate */
    sr = sonilo_srate(mem);

    for (n = 0; n < 64; n++) {
        float f, a, o;
        f = sonilo_port_read(&freq, n);
        a = sonilo_port_read(&amp, n);
        /* TODO: compute sample of audio */
        o = 0.0;

        /* TODO: update state */
        state->phs = 0;
        sonilo_port_write(&out, n, o);
    }

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

    s = malloc(sonilo_sizeof());
    sonilo_init(s);
    sonilo_ctx_init(&ctx, s);

    /* push amp/freq constants */
    sonilo_constant(&ctx, 440);
    sonilo_constant(&ctx, 0.5);

    /* bind DSP command to sonilo */
    ukey = sonilo_key("SIN");
    sonilo_command(s, ukey, render);

    /* allocate/initialize ugen */
    sonilo_ugen_init(&ctx, &ugen, ukey, 3, 4);

    /* set up ports: two inputs, one output */
    sonilo_iport(&ugen, 0);
    sonilo_iport(&ugen, 1);
    sonilo_oport(&ugen, 2);

    /* perform GC to return any freed buffers */
    sonilo_clean(&ctx);

    /* initialize ugen internal state */
    state = (sine_data *)ugen.state;
    state->phs = 0;

    /* get output of ugen */
    rc = sonilo_ugen_block(&ugen, 2, &out);

    if (rc) {
        /* not a block, for some reason */
        printf("not a block, for some reason");
        goto clean;
    }

    fp = fopen("out.raw", "wb");
    for (i = 0; i < 3500; i++) {
        sonilo_ugen_compute(&ugen);
        /* write contents of output to disk */
        fwrite(out, sizeof(float), 64, fp);
    }

    /* cleanup */
clean:
    sonilo_ctx_destroy(&ctx);
    free(s);
    fclose(fp);
    return 0;
}
