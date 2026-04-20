#include <stdint.h>
#include <stddef.h>
#include "sonilo.h"

/* 30-bit Q16.13 with sign bit */
static float qtof(uint32_t w)
{
    uint8_t s;
    uint16_t i;
    uint16_t f;
    float o;

    /* fractional component: 13 bits */
    f = w & 0x1FFF;
    w >>= 13;
    /* integer components: 16 bits */
    i = w & 0xFFFF;
    w >>= 16;

    /* sign bit */
    s = w & 1;

    /* use 2^13 as divisor. the max bits should never
     * equal 1 exactly */
    o = (s ? -1 : 1)*(i + ((float)f / 0x2000));

    return o;
}

static uint32_t ftoq(float x)
{
    uint16_t i;
    uint16_t f;
    uint8_t s;
    uint32_t o;

    if (x < 0) {
        s = 1;
        /* negate to make positive */
        x *= -1;
    } else {
        s = 0;
    }

    i = (uint16_t)x;
    i &= 0xFFFF;
    f = ((x - i) * 0x2000);
    f &= 0x1FFF;

    o = s;
    o <<= 16;
    o |= i;
    o <<= 13;
    o |= f;

    return o;
}

sonilo_port sonilo_port_from_word(uint32_t *mem, uint32_t w)
{
    int type;
    uint32_t data;
    sonilo_port p;

    type = w & 3;
    data = w >> 2;

    if (type == PORT_CONSTANT) {
        p.type = PORT_CONSTANT;
        p.data.constant = qtof(data);
    } else if (type == PORT_BLOCK) {
        p.type = PORT_BLOCK;
        p.data.block.block = (float *)&mem[data & 0xFFFF];
        p.data.block.addr = data & 0xFFFF;
    } else {
        p.type = PORT_CONSTANT;
        p.data.constant = 0;
    }

    return p;
}

uint32_t sonilo_port_to_word(uint32_t *mem, sonilo_port *p)
{
    uint32_t w;
    w = 0;

    if (p->type == PORT_BLOCK) {
        w = PORT_BLOCK;
        w |= (p->data.block.addr) << 2;
    } else if (p->type == PORT_CONSTANT) {
        w = PORT_CONSTANT;
        w |= (ftoq(p->data.constant)) << 2;
    }

    return w;
}

float sonilo_port_read(sonilo_port *p, int n)
{
    if (p->type == PORT_CONSTANT) {
        return p->data.constant;
    }

    if (p->type == PORT_BLOCK) {
        if (n < 0 || n >= 64) return 0;
        return p->data.block.block[n];
    }

    return 0;
}

void sonilo_port_write(sonilo_port *p, int n, float s)
{
    if (p->type == PORT_CONSTANT) {
        p->data.constant = s;
    } else if (p->type == PORT_BLOCK) {
        if (n < 0 || n >= 64) return;
        p->data.block.block[n] = s;
    }
}

sonilo_port sonilo_port_constant(float c)
{
    sonilo_port p;
    p.type = PORT_CONSTANT;
    p.data.constant = c;
    return p;
}

sonilo_port sonilo_port_block(uint32_t *mem, uint32_t p)
{
    sonilo_port port;

    port.type = PORT_BLOCK;
    port.data.block.addr = p;
    port.data.block.block = (float *)&mem[p];

    return port;
}
