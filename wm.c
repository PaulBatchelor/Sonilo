#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* sonilo stubs for the parser */
typedef struct sonilo_vm sonilo_vm;
int sonilo_op_a(sonilo_vm *vm, char type, uint8_t data);
int sonilo_op_b(sonilo_vm *vm, char type, uint32_t data);

#include "wm.h"

/* word machine states */
/* An explanation for state names.
 * Variable names are hard for state names.
 * Instead, look at it this way.
 * 'M' is a single element map.
 * 'C' is a character.
 * 'U7' is a 7-bit unsigned integer.
 * 'U32' is a 32-bit unsigned integer.
 * The state machine can be tersely notated like this:
 *     M->C->(U7, U32).
 * With an error E, it can be written like so:
 *     (M, E)->(C, E)->(U7, U32, E)
 * Which correspond to distinct states S:
 *     (S0, E)->(S1, E)->(S2, S3, E)
 */
enum {
    ST_ERR,
    ST_INIT,
    ST_0,
    ST_1,
    ST_2,
    ST_3
};

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
    /* NOTE: HW is already an BE encoded word */

    if (wm->pb >= 64) return 1;

    /* NOTE: on LE host system, MSB/LSB are flipped */
    if (which) {
        /* 1: set LSB of BE word */

        uint32_t w;

        w = wm->buf[wm->pb];

        w &= 0xFFFF;
        w |= hw << 16;

        /* set AND advance the buffer pointer.
         * this enforces an ordering: set the MSB,
         * then set the LSB */

        wm->buf[wm->pb] = w;
        wm->pb++;
    } else {
        /* 0: set MSB of BE word */

        uint32_t w;

        w = wm->buf[wm->pb];
      
        w &= ~0xFFFF;
        w |= hw;

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

int word_machine_parse(word_machine *wm, cmp_ctx_t *cmp, sonilo_vm *vm)
{
    cmpbuf buf;
    uint32_t st;
    char subtype;
    char sbuf[12] = {0};
    uint32_t data;
    int rc;
    int read_input;

    buf.pos = 0;
    buf.buf = (uint8_t *)wm->buf;
    buf.len = wm->pb << 2;
    cmp_init(cmp, &buf, bufreader, NULL, NULL);

    st = ST_INIT;
    subtype = '\0';
    data = 0;
    read_input = 1;

    while (1) {
        cmp_object_t obj;

        if (read_input && !cmp_read_object(cmp, &obj)) {
            if (buf.pos >= buf.len) break;
            else return 1;
        }

        /* see comment in enum declaration for info on stat emachine */
        switch(st) {
            case ST_ERR:
                return 1;
            case ST_INIT: /* look for maps of size 1 (M) */
                switch(obj.type) {
                    case CMP_TYPE_FIXMAP:
                    case CMP_TYPE_MAP16:
                    case CMP_TYPE_MAP32:
                        if (obj.as.map_size == 1) {
                            st = ST_0;
                        } else {
                            st = ST_ERR;
                        }
                        break;
                    default:
                        st = ST_ERR;
                        break;
                }
                break;
            case ST_0:
                /* look for subtype: a single-character string */
                switch(obj.type) {
                    case CMP_TYPE_FIXSTR:
                    case CMP_TYPE_STR8:
                    case CMP_TYPE_STR16:
                    case CMP_TYPE_STR32:
                        if (!read_bytes(sbuf, obj.as.str_size, &buf)) {
                            return 2;
                        }
                        sbuf[obj.as.str_size] = 0;
                        if (obj.as.str_size == 1) {
                            subtype = sbuf[0];
                            st = ST_1;
                        } else {
                            st = ST_ERR;
                        }
                        break;
                    default:
                        st = ST_ERR;
                        break;
                }
                break;
            case ST_1:
                /* extract 7-bit or 32-bit data component */
                /* next state is terminal, don't read input
                 * of next token */
                read_input = 0;
                switch(obj.type) {
                    case CMP_TYPE_POSITIVE_FIXNUM:
                        data = obj.as.u8;
                        st = ST_2;
                        break;
                    case CMP_TYPE_UINT32:
                        data = obj.as.u32;
                        st = ST_3;
                        break;
                    default:
                        st = ST_ERR;
                        break;
                }
                break;
            case ST_2:
                /* process A-form word */
                rc = sonilo_op_a(vm, subtype, data);
                if (rc) return 3;
                /* reset */
                read_input = 1;
                st = ST_INIT;
                break;
            case ST_3:
                /* process B-form word */
                rc = sonilo_op_b(vm, subtype, data);
                if (rc) return 4;

                /* reset */
                read_input = 1;
                st = ST_INIT;
                break;
            default:
                break;
        }
    }
    return 0;
}
