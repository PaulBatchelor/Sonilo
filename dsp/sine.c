#include "sonilo.h"
#include "mem.h"
#include "context.h"

static uint32_t init(uint32_t *mem, uint16_t ctx)
{
    int rc;
    uint16_t stk;
    uint16_t key;
    int cmd;

    stk = CTX_STACK(mem, ctx);
    key = 0;
    rc = array_pop(mem, stk, &key);
    if (rc) return 1;

    return 0;
}

static uint32_t render(uint32_t *mem, uint16_t p)
{
    /* TODO */
    return 1;
}

int ugen_sine(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SIN");

    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
