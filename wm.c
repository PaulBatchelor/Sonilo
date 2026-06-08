#include <stdint.h>
#include <stddef.h>
#include "wm.h"

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
