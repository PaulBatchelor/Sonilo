#include <stdint.h>
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
int word_machine_send(word_machine *wm, char c)
{

    /* TODO: implement */
    return 1;
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
