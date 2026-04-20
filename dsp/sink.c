#include "sonilo.h"

static uint32_t init(uint32_t *mem, uint16_t p)
{
    /* TODO */
    return 1;
}

static uint32_t render(uint32_t *mem, uint16_t p)
{
    /* TODO */
    return 1;
}

int ugen_sink(sonilo *s)
{
    uint16_t key;
    int rc;

    key = sonilo_key("SNK");

    rc = sonilo_command(s, key, init);
    if (rc) return 1;
    rc = sonilo_command(s, sonilo_alt(key), render);
    if (rc) return 2;

    return 0;
}
