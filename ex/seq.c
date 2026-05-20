#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"
#include "array.h"
#include "context.h"
#include "iter.h"

int render(sonilo_ctx *ctx, uint16_t sink)
{
    int rc;
    sonilo *s;
    float *out;
    int i;
    FILE *fp;
    uint32_t *mem;

    fp = fopen("seq.raw", "wb");

    s = ctx->s;
    mem = sonilo_mem(s);
    rc = sonilo_ugen_block(mem, sink, 0, &out);
    if (rc) return 1;

    for (i = 0; i < 3445*2; i++) {
        rc = sonilo_process(ctx);
        if (rc) return 1;
        fwrite(out, sizeof(float), 64, fp);
    }

    fclose(fp);

    return 0;
}
const int sequence[] = {
    0, 5, 7, 10, 12, 10, 7, 5,
    0, 5, 7, 10, 12, 10, 7, 5,
    -2, 3, 5, 8, 10, 8, 5, 3,
    -2, 3, 5, 8, 10, 8, 5, 3,
};

int mkseq(sonilo_ctx *ctx)
{
    uint32_t args;
    uint32_t val;
    uint16_t arr, iter, stk;
    int rc;
    int i;
    uint32_t *mem;

    mem = sonilo_mem(ctx->s);

    stk = CTX_STACK(mem, ctx->context);
    /* create an array of 16 8-bit (2^3) values */
    /* array args are packed in a word: len.wrdsz */
    args = 3 | (32 << 4);
    rc = sonilo_push(ctx, args);
    if (rc) return 1;

    /* push context address (needed for array) */
    rc = sonilo_push(ctx, ctx->context);
    if (rc) return 2;

    rc = array_create(mem, stk);
    if (rc) return 3;

    /* get the address from the stack */
    val = 0;
    rc = sonilo_pop(ctx, &val);
    if (rc) return 4;
    arr = val;

    /* set up sequence values */
    for (i = 0; i < 32; i++) {
        rc = sonilo_push(ctx, sequence[i] + 60);
        if (rc) return 5;
        rc = sonilo_push(ctx, i);
        if (rc) return 6;
        rc = sonilo_push(ctx, arr);
        if (rc) return 7;
        rc = array_write(mem, stk);
        if (rc) return 8;
    }

    /* create an array iterator */
    iter = 0;
    rc = iter_alloc(mem, ctx->context, &iter);
    if (rc) return 9;
    rc = iter_init(mem, iter);
    if (rc) return 10;
    rc = iter_array(mem, iter, arr);
    if (rc) return 11;

    /* push iterator onto stack */
    rc = sonilo_push(ctx, iter);
    if (rc) return 12;

    /* create sequencer ugen */
    rc = sonilo_mkugen(ctx, "SEQ");
    if (rc) return 13;
    return 0;
}

int main(int argc, char *argv[])
{
    sonilo *s;
    sonilo_ctx ctx;
    int rc;
    uint16_t sink;

    s = malloc(sonilo_sizeof());
    rc = sonilo_init(s);
    if (rc) goto clean;
    rc = sonilo_ctx_init(&ctx, s);
    if (rc) goto clean;

    /* clock */
    rc = sonilo_constant(&ctx, 125 * 4);
    if (rc) goto clean;
    rc = sonilo_mkugen(&ctx, "CLK");
    if (rc) goto clean;
    rc = sonilo_mkugen(&ctx, "MET");
    if (rc) goto clean;

    /* sequencer driven by clock */
    rc = mkseq(&ctx);
    if (rc) goto clean;

    /* smoother on pitch signal */
    rc = sonilo_constant(&ctx, 0.005);
    if (rc) goto clean;
    rc = sonilo_mkugen(&ctx, "SMO");
    if (rc) goto clean;

    /* midi to frequency */
    rc = sonilo_mkugen(&ctx, "MTF");
    if (rc) goto clean;

    /* saw, controlled via freq signal */
    rc = sonilo_mkugen(&ctx, "SAW");
    if (rc) goto clean;

    rc = sonilo_constant(&ctx, 200);
    if (rc) goto clean;
    /* filter saw with LPF */
    rc = sonilo_mkugen(&ctx, "LPF");
    if (rc) goto clean;

    rc = sonilo_constant(&ctx, 0.7);
    if (rc) goto clean;
    rc = sonilo_mkugen(&ctx, "MUL");
    if (rc) goto clean;

    /* output */
    rc = sonilo_mkugen(&ctx, "SNK");
    if (rc) goto clean;
    sink = 0;
    rc = sonilo_last_ugen(&ctx, &sink);
    if (rc) goto clean;
    /* render  */
    rc = render(&ctx, sink);

    if (rc) goto clean;

    clean:
    if (rc) {
        fprintf(stderr, "sonilo error: %d\n", rc);
    }

    sonilo_ctx_destroy(&ctx);
    free(s);

    return 0;
}
