#include <stdio.h>

#include "sonilo.h"

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    sonilo_ugen ugen;

    s = malloc(sonilo_sizeof());
    sonilo_init(s);
    sonilo_ctx_init(&ctx, s);

    /* create ugen */
    sonilo_ugen_init(&ctx, &ugen, 2);

    /* set up ports */

    /* allocate ugen data and setup DSP callback */

    /* get output of ugen */

    /* compute some blocks and write to disk */

    /* cleanup */
    sonilo_ctx_destroy(&ctx);
    free(s);
    return 0;
}
