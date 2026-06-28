#include <stdlib.h>
#include <stdio.h>
#include "sonilo.h"
#include "array.h"
#include "mem.h"
#include "context.h"
#include "iter.h"
#define OPB(CMD,DAT) sonilo_op_b(s, CMD, DAT)
#define OPA(CMD,DAT) sonilo_op_a(s, CMD, DAT)

int render(sonilo *s, uint16_t ctx, uint16_t sink)
{
    int rc;
    float *out;
    int i;
    FILE *fp;
    uint32_t *mem;

    fp = fopen("opseq.raw", "wb");

    mem = sonilo_mem(s);
    rc = sonilo_ugen_block(mem, sink, 0, &out);
    if (rc) return 1;

    for (i = 0; i < 3445*2; i++) {
        rc = sonilo_process(s, ctx);
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

int mkseq(sonilo *s, uint16_t ac)
{
    uint32_t args;
    uint32_t val;
    uint16_t arr;
    int rc;
    int i;

    /* create an array of 16 8-bit (2^3) values */
    /* array args are packed in a word: len.wrdsz */
    args = 3 | (32 << 4);
    rc = OPB('w', args);
    if (rc) return 1;

    /* push context address (needed for array) */
    rc = OPB('w', ac);
    if (rc) return 2;

    /* create array */
    rc = OPA('a', 0);
    if (rc) return 3;

    /* pop the address from the stack */
    rc = OPA('w', 0);
    if (rc) return 4;
    val = 0;
    sonilo_get(s, &val);
    arr = val;

    /* set up sequence values */
    for (i = 0; i < 32; i++) {
        rc = OPB('w', sequence[i] + 60);
        if (rc) return 5;
        rc = OPB('w', i);
        if (rc) return 6;
        rc = OPB('w', arr);
        if (rc) return 7;

        /* write array */
        rc = OPA('a', 1);
        if (rc) return 8;
    }

    /* create an array iterator */

    rc = OPB('w', arr);
    rc = OPA('i', 0);
    if (rc) return 11;

    /* create sequencer ugen */
    rc = OPB('u', sonilo_key("SEQ"));
    if (rc) return 13;
    return 0;
}

int main(int argc, char *argv[])
{
    sonilo *s;
    int rc;
    uint16_t sink;
    sonilo_vm *vm;
    uint16_t ac;
    uint32_t val;

    s = NULL;
    rc = sonilo_create(&s);
    if (rc) goto clean;

    vm = sonilo_get_vm(s);
    /* create context */
    OPA('C', 0);
    rc = sonilo_get(s, &val);
    if (rc) goto clean;
    ac = val;
    sonilo_vm_cursor_select(vm, 0);
    sonilo_vm_cursor_set(vm, ac);

    /* clock */
    rc = OPB('c', sonilo_ftoq(125 * 4));
    if (rc) goto clean;
    rc = OPB('u', sonilo_key("CLK"));
    if (rc) goto clean;
    rc = OPB('u', sonilo_key("MET"));
    if (rc) goto clean;

    /* sequencer driven by clock */
    rc = mkseq(s, ac);
    if (rc) goto clean;

    /* smoother on pitch signal */
    rc = OPB('c', sonilo_ftoq(0.005));
    if (rc) goto clean;
    rc = OPB('u', sonilo_key("SMO"));
    if (rc) goto clean;

    /* midi to frequency */
    rc = OPB('u', sonilo_key("MTF"));
    if (rc) goto clean;

    /* saw, controlled via freq signal */
    rc = OPB('u', sonilo_key("SAW"));
    if (rc) goto clean;

    /* filter saw with LPF */
    rc = OPB('c', sonilo_ftoq(200));
    if (rc) goto clean;
    rc = OPB('u', sonilo_key("LPF"));
    if (rc) goto clean;

    rc = OPB('c', sonilo_ftoq(0.7));
    if (rc) goto clean;
    rc = OPB('u', sonilo_key("MUL"));
    if (rc) goto clean;

    /* output */
    rc = OPB('u', sonilo_key("SNK"));
    if (rc) goto clean;
    sink = 0;
    /* get last ugen */
    rc = OPA('u', 0);
    if (rc) goto clean;
    val = 0;
    rc = sonilo_get(s, &val);
    if (rc) goto clean;
    sink = val;

    rc = sonilo_tape_open(s, 0);
    if (rc) goto clean;
    rc = sonilo_tape_bind(s, 0, sink);
    if (rc) goto clean;
    rc = sonilo_render(s, ac, 10);
    if (rc) goto clean;

    clean:
    if (rc) {
        fprintf(stderr, "sonilo error: %d\n", rc);
    }

    sonilo_tape_close(s, 0);
    /* destroy context */
    OPA('C', 1);
    sonilo_destroy(s);

    return 0;
}
