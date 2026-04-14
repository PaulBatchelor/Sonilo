#include <stdio.h>
#include "sonilo.h"

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    sonilo_ugen ugen;
    uint32_t i;

    s = malloc(sonilo_sizeof());
    sonilo_init(s);
    sonilo_ctx_init(&ctx, s);

    /* push amp/freq constants */
    sonlio_constant(&ctx, 440);
    sonlio_constant(&ctx, 0.5);

    /* create ugen */
    sonilo_ugen_init(&ctx, &ugen, 3, 0);

    /* set up ports: two inputs, one output */
    sonilo_iport(&ugen, 0);
    sonilo_iport(&ugen, 1);
    sonilo_oport(&ugen, 2);

    /* perform GC to return any freed buffers */
    sonilo_clean(&ctx);

    /* allocate ugen data and setup DSP callback */

    /* get output of ugen */

    for (i = 0; i < 3500; i++) {
        sonilo_ugen_compute(&ugen);
        /* write contents of output to disk */
    }

    /* cleanup */
    sonilo_ctx_destroy(&ctx);
    free(s);
    return 0;
}
