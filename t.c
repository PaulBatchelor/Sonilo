#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem.h"

#define BLOCKLIST_OFFSET 0x400

static volatile int running = 0;

enum {
    PRINT,
    COMMENT,
    MEMWRITE,
    UPDATE,
    LABEL,
    QUIT
};

int update_mode(char c) {
    switch (c) {
        case 'p':
            return PRINT;
        case 'c':
            return COMMENT;
        case 'm':
            return MEMWRITE;
        case 'l':
            return LABEL;
        case 'q':
            running = 0;
            return QUIT;
    }

    return PRINT;
}

typedef struct memwrite {
    /* word register */
    uint32_t rw; 
    /* previous character */
    char prev;
    /* cursor */
    uint16_t cursor;

    /* memory */
    uint32_t mem[0x10000];

    /* errors */
    uint32_t err;
} memwrite;

void memwrite_init(memwrite *mw)
{
    uint32_t i;
    mw->rw = 0;
    mw->prev = 0;
    for (i = 0; i < 0x10000; i++) mw->mem[i] = 0;
    mw->err = 0;
}

static uint32_t reverse_nibbles(uint32_t w)
{
    w = (w & 0x0F0F0F0F) << 4 | (w & 0xF0F0F0F0) >> 4;
    w = (w & 0x00FF00FF) << 8 | (w & 0xFF00FF00) >> 8;
    w = (w & 0x0000FFFF) << 16 | (w & 0xFFFF0000) >> 16;
    return w;
}

void parse_memwrite(memwrite *mw, char c)
{
#ifdef DEBUG
        fputc(c, stdout);
#endif
    /* process nibbles */
    if (c >= '0' && c <= '9') {
        uint8_t x;
        uint32_t w;
        w = mw->rw;
        x = (uint8_t)(c - '0');
        w <<= 4;
        w |= x;
        mw->rw = w;
        return;
    } else if (c >= 'A' && c <= 'F') {
        uint8_t x;
        uint32_t w;
        w = mw->rw;
        x = (uint8_t)(c - 'A');
        x += 10;
        w <<= 4;
        w |= x;
        mw->rw = w;
        return;
    }

    if (c == ' ' || c == '\n') return;

    if (mw->prev == 'p' && c == 'r') {
        printf("%x\n", mw->rw);
        mw->prev = 0;
        return;
    }

    if (mw->prev == 'r' && c == 'v') {
        mw->rw = reverse_nibbles(mw->rw); 
        mw->prev = 0;
        return;
    }

    /* cl or '#': clear the word register */
    if (c == '#' || (mw->prev == 'c' && c == 'l')) {
        mw->rw = 0;
        mw->prev = 0;
        return;
    }

    /* go: set cursor location */
    if (mw->prev == 'g' && c == 'o') {
        mw->prev = 0;
        mw->cursor = mw->rw & 0xFFFF;
        return;
    }

    /* wr: write word to memory */
    if (mw->prev == 'w' && c == 'r') {
        mw->mem[mw->cursor] = mw->rw;
        mw->prev = 0;
        return;
    }

    /* read word from memory to word register */
    if (mw->prev == 'r' && c == 'd') {
        mw->rw = mw->mem[mw->cursor];
        mw->prev = 0;
        return;
    }

    /* bi: init blocklist */
    if (mw->prev == 'b' && c == 'i') {
        blocklist_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* ba: allocate block, write block id to word register */
    if (mw->prev == 'b' && c == 'a') {
        mw->rw = blocklist_pop(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* bf: free block, read block id from register word */
    if (mw->prev == 'b' && c == 'f') {
        blocklist_push(mw->mem, mw->cursor, mw->rw & 0x3ff);
        mw->prev = 0;
        return;
    }

    /* si: initialize bitset */
    if (mw->prev == 's' && c == 'i') {
        bitset_init(mw->mem, mw->cursor);
        mw->prev = 0;
        return;
    }

    /* sa: add item to bitset */
    if (mw->prev == 's' && c == 'a') {
        bitset_add(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* se: check to see if item exists in set */
    if (mw->prev == 's' && c == 'e') {
        mw->rw = bitset_exists(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* sr: remove item from bitset */
    if (mw->prev == 's' && c == 'r') {
        bitset_remove(mw->mem, mw->cursor, mw->rw);
        mw->prev = 0;
        return;
    }

    /* rb: read word from block */
    if (mw->prev == 'r' && c == 'b') {
        int offset;
        mw->prev = 0;
        /* 6-bit address 0 - 63 */
        offset = mw->rw & 0x3f;
        mw->rw = mw->mem[mw->cursor + offset];
        return;
    }

    /* mi: initialize memory */
    if (mw->prev == 'm' && c == 'i') {
        uint32_t *stk;
        uint16_t p_top, p_block, p_avail, p_tags;
        uint8_t sp;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 4) {
            mw->err = 1;
            return;
        }

        /* pop off stack */
        p_tags = stk[sp]; sp--;
        p_avail = stk[sp]; sp--;
        p_block = stk[sp]; sp--;
        p_top = stk[sp]; sp--;

        stk[0] = sp;

        mem_init(mw->mem, p_top, p_block, p_avail, p_tags);
        return;
    }

    /* gb: goto block */
    if (mw->prev == 'g' && c == 'b') {
        /* multiply by 64 to get word address,
         * then add an offset to skip the block list
         * block list size: 2^16/64 = 1024. 1024 blocks = 1024 words
         */
        mw->cursor = (mw->rw << 6) + BLOCKLIST_OFFSET; 
        mw->prev = 0;
        return;
    }

    /* ma: allocate a block */
    if (mw->prev == 'm' && c == 'a') {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, k;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 1;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        stk[0] = sp;

        mw->rw = mem_alloc(mw->mem, p_top, k);
        return;
    }

    /* mf: free a block */
    if (mw->prev == 'm' && c == 'f') {
        uint8_t sp;
        uint32_t *stk;
        uint16_t p_top, L, k;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 3) {
            mw->err = 1;
            return;
        }
        p_top = stk[sp]; sp--;
        k = stk[sp]; sp--;
        L = stk[sp]; sp--;
        stk[0] = sp;

        mem_free(mw->mem, p_top, L, k);
        return;
    }

    /* pw: print word */
    if (mw->prev == 'p' && c == 'w') {
        printf("%x", mw->rw);
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ai: initialize array */
    if (mw->prev == 'a' && c == 'i') {
        int n;
        for (n = 0; n < 64; n++) mw->mem[mw->cursor + n] = 0;
        mw->prev = 0;
        return;
    }
    /* al: get array length */
    if (mw->prev == 'a' && c == 'l') {
        /* first word in block stores length */
        mw->rw = mw->mem[mw->cursor];
        mw->prev = 0;
        return;
    }


    /* sp: display space character */
    if (mw->prev == 's' && c == 'p') {
        putchar(' ');
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* aa: append value to array */
    if (mw->prev == 'a' && c == 'a') {
        int pos;
        mw->prev = 0;
        pos = mw->mem[mw->cursor];
        /* first word in block stores length */
        mw->mem[mw->cursor + pos + 1] = mw->rw;
        mw->mem[mw->cursor] = pos + 1;
        return;
    }

    /* ap: pop word from array */
    if (mw->prev == 'a' && c == 'p') {
        int pos;
        pos = mw->mem[mw->cursor];
        mw->prev = 0;
        if (pos == 0) {
            mw->err = 1;
            return;
        }
        /* first word in block stores length */
        mw->rw = mw->mem[mw->cursor + pos];
        mw->mem[mw->cursor] = pos - 1;
        return;
    }

    /* pe: print error flag */
    if (mw->prev == 'p' && c == 'e') {
        printf("%x", mw->err);
        fflush(stdout);
        mw->prev = 0;
        return;
    }

    /* ce: clear error flag */
    if (mw->prev == 'c' && c == 'e') {
        mw->prev = 0;
        mw->err = 0;
        return;
    }

    /* bw: convert block.offset #BBBOO notation into
     * a word address */
    if (mw->prev == 'b' && c == 'w') {
        uint16_t addr;
        mw->prev = 0;

        addr = mw->rw;
        mw->rw = (addr >> 8) * 64 + (addr & 63);

        /* see: 'gb' */
        mw->rw += BLOCKLIST_OFFSET;

        return;
    }

    /* rc: read cursor into rw */
    if (mw->prev == 'r' && c == 'c') {
        mw->rw = mw->cursor;
        mw->prev = 0;
        return;
    }

    /* mc: generate 32-bit checksum of memory allocator */
    if (mw->prev == 'm' && c == 'c') {
        mw->rw = mem_cksum(mw->mem, mw->rw);
        mw->prev = 0;
        return;
    }

    /* bo: calculate block offset from word address */
    if (mw->prev == 'b' && c == 'o') {
        mw->prev = 0;
        /* assuming blocks are aligned, last 6 bits
         * should be the offset
         */
        mw->rw &= 63; 
        return;
    }

    /* ad: add two numbers from a stack, store in word register */
    if (mw->prev == 'a' && c == 'd') {
        uint8_t sp;
        uint32_t *stk;
        uint16_t x, y;
        mw->prev = 0;

        stk = &mw->mem[mw->cursor];
        sp = stk[0];

        if (sp < 2) {
            mw->err = 1;
            return;
        }

        y = stk[sp]; sp--;
        x = stk[sp]; sp--;

        mw->rw = x + y;

        stk[0] = sp;
        return;
    }
    /* TODO su: subtract two numbers from a stack, store in word register */
    /* TODO di : divide two numbers from a stack, store in word register */
    /* TODO mu : multiply two numbers from a stack, store in word register */

    mw->prev = c;
}

int main (int argc, char *argv[])
{
    int mode;
    memwrite *mw;

    mw = malloc(sizeof(memwrite));
    mode = PRINT;
    memwrite_init(mw);
    running = 1;
    while (!feof(stdin) && running) {
        char c;
        c = fgetc(stdin);

        /* handle FEOF character */
        if (c == -1) continue;

        if (mode == UPDATE) {
            mode = update_mode(c);
            continue;
        }

        if (c == '@') {
            mode = UPDATE;
            continue;
        }

        if (mode == COMMENT) {
            /* ignore comment for now */
            continue;
        }

        if (mode == MEMWRITE) {
            parse_memwrite(mw, c);
            continue;
        }

        if (mode == LABEL) {
            if (c != ' ' && c != '\n') fputc(c, stdout);
            continue;
        }

        /* print mode */
        fputc(c, stdout);
    }
    free(mw);
    return 0;
}
