#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "wm.h"

typedef struct cmpbuf {
    uint16_t pos;
    uint16_t len;
    uint8_t *buf;
} cmpbuf;

struct word_machine {
    /* input word */
    uint32_t w;
    /* word/buf position counters */
    uint16_t pw;
    uint16_t pb;

    /* pointer to sonilo VM */
    uint16_t vm;

    /* reserved */
    uint16_t reserved;
    
    /* buffer */
    uint32_t buf[64];
};

void word_machine_init(word_machine *wm, uint16_t vm)
{
    int i;
    wm->w = 0;
    wm->pw = wm->pb = 0;
    for (i = 0; i < 64; i++) wm->buf[i] = 0;
    wm->vm = vm; 
}

/* send: send byte to word machine */
int word_machine_send(word_machine *wm, unsigned char c)
{
    uint32_t w;

    /* bounds checking */
    if (wm->pw >= 4) return 1;

    w = wm->w;

    /* clear slot and write */

    w &= ~(0xFF << (8 * wm->pw));
    w |= c << (8 * wm->pw);

    wm->w = w;
    wm->pw++;

    return 0;
}

/* begin: begin a new message */
int word_machine_begin(word_machine *wm)
{
    /* TODO: implement */
    return 1;
}

/* end: parse bytes in buffer and clear message */
int word_machine_end(word_machine *wm)
{
    /* TODO: implement */
    return 1;
}

int word_machine_wpos(word_machine *wm)
{
    return wm->pw;
}

int word_machine_bpos(word_machine *wm)
{
    return wm->pb;
}

void word_machine_clear(word_machine *wm)
{
    wm->pw = 0;
    wm->w = 0;
}

uint32_t word_machine_word(word_machine *wm)
{
    return wm->w;
}

int word_machine_issys(uint32_t w)
{
    unsigned char *b;

    /* interpret bytes in LE system */

    /* TODO: maybe work out something more endian-agnostic */
    b = (unsigned char *)&w;

    /* System commands are 1-sized fixmaps with a fixint key */
    if (b[0] == 0x81 && (b[1] >> 7) == 0) {
        return 1;
    }

    return 0;
}

int word_machine_extract_sys(uint32_t w, uint8_t *k, uint8_t *v)
{
    unsigned char *b;

    if (k == NULL || v == NULL) return 1;

    b = (unsigned char *)&w;

    *k = b[1];
    *v = b[3];

    return 0;
}

int word_machine_append(word_machine *wm, uint32_t w)
{
    if (wm->pb >= 64) return 1;
    wm->buf[wm->pb] = w;
    wm->pb++;
    return 0;
}

int word_machine_append_half(word_machine *wm, uint16_t hw, int which)
{
    if (wm->pb >= 64) return 1;
    if (which) {
        /* 1: set LSB */

        uint32_t w;

        w = wm->buf[wm->pb];
        w &= ~0xFFFF;
        w |= hw;

        /* set AND advance the buffer pointer.
         * this enforces an ordering: set the MSB,
         * then set the LSB */

        wm->buf[wm->pb] = w;
        wm->pb++;
    } else {
        /* 0: set MSB */

        uint32_t w;

        w = wm->buf[wm->pb];

        w &= ~0xFFFF;
        w |= hw << 16;

        wm->buf[wm->pb] = w;
    }

    return 0;
}

int word_machine_reset(word_machine *wm)
{
    wm->pw = 0;
    wm->pb = 0;
    wm->w = 0;
    return 0;
}


static bool read_bytes(void *data, size_t sz, cmpbuf *buf) {
    uint16_t pos;
    uint16_t i;
    uint8_t *a;

    pos = buf->pos;
    if ((pos + sz) > buf->len) {
        sz = buf->len - pos;
    }

    a = (uint8_t *)data;
    for (i = 0; i < sz; i++) a[i] = buf->buf[pos + i];

    buf->pos += sz;
    return sz > 0;
}

static bool bufreader(cmp_ctx_t *ctx, void *data, size_t limit) {
    return read_bytes(data, limit, ctx->buf);
}

int word_machine_parse(word_machine *wm, cmp_ctx_t *cmp)
{
    cmpbuf buf;
    buf.pos = 0;
    buf.buf = (uint8_t *)wm->buf;
    buf.len = wm->pb << 2;
    cmp_init(cmp, &buf, bufreader, NULL, NULL);

    while (1) {
        cmp_object_t obj;

        if (!cmp_read_object(cmp, &obj)) {
            if (buf.pos >= buf.len) break;
            else return 1;

        }
        switch(obj.type) {
            case CMP_TYPE_FIXMAP:
            case CMP_TYPE_MAP16:
            case CMP_TYPE_MAP32:
                printf("Map: %u\n", obj.as.map_size);
                break;
            case CMP_TYPE_UINT32:
                printf("Unsigned Integer: %u\n", obj.as.u32);
                break;
            case CMP_TYPE_POSITIVE_FIXNUM:
                printf("FIXINT: %u\n", obj.as.u8);
                break;
            case CMP_TYPE_UINT8:
                printf("Unsigned Integer: %u\n", obj.as.u8);
                break;
        }
    }
    return 0;
}
