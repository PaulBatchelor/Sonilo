#include <stdint.h>
#include <stdio.h>
#include "mem.h"
#define AVAIL_OFFSET 64
#define LOC_AVAIL(K) (K + AVAIL_OFFSET)
#define TAGS(MEM, TOP) (MEM[TOP + 1])
#define AVAIL(MEM, TOP) (MEM[TOP] & 0xFFFF)
#define BLOCK(MEM, TOP) ((MEM[TOP] >> 16) & 0xFFFF)

static uint32_t linkf_set(uint32_t w, int a)
{
    w &= ~(127);
    w |= (a & 127);
    return w;
}

static int linkf_get(uint32_t w)
{
    return (w) & 127;
}

static uint32_t linkb_set(uint32_t w, int a)
{
    w &= ~(127 << 7);
    w |= (a & 127) << 7;
    return w;
}

static int linkb_get(uint32_t w)
{
    return (w >> 7) & 127;
}

static uint32_t *get_avail(uint32_t *mem, uint16_t p_top)
{
    uint16_t p_avail;
    p_avail = AVAIL(mem, p_top);
    return &mem[p_avail];
}

static void availf_set_v2(uint32_t *avail, int k, int p)
{
    avail[k] = linkf_set(avail[k], p);
}

static int availf_get_v2(uint32_t *avail, int k) 
{
    return linkf_get(avail[k]);
}

static void availb_set_v2(uint32_t *avail, int k, int p)
{
    avail[k] = linkb_set(avail[k], p);
}

static int availb_get_v2(uint32_t *avail, int k) 
{
    return linkb_get(avail[k]);
}

static void availf_set(uint32_t *avail, int k, int p)
{
    availf_set_v2(avail, k, p);
}

static int availf_get(uint32_t *avail, int k) 
{
    return availf_get_v2(avail, k);
}

static void availb_set(uint32_t *avail, int k, int p)
{
    availb_set_v2(avail, k, p);
}

static int availb_get(uint32_t *avail, int k)
{
    return availb_get_v2(avail, k);
}

static void kval_set(uint32_t *mem, uint16_t p_top, int p, int k)
{
    uint32_t *block;
    uint16_t p_block;

    p_block = BLOCK(mem, p_top);
    block = &mem[p_block];
    block[p] &= ~(7 << 14);
    block[p] |= (k & 7) << 14;
}

static int kval_get(uint32_t *mem, uint16_t p_top, int p)
{
    uint32_t *block;
    uint16_t p_block;
    p_block = BLOCK(mem, p_top);
    block = &mem[p_block];
    return (block[p] >> 14) & 7;
}

static void tag_set(uint32_t *tags, int p, int tag)
{
    int w;
    w = 0;
    if (p >= 32) {
       w = 1;
       p -= 32;
    }

    tags[w] &= ~(1 << p);
    tags[w] |= tag << p;
}

static int tag_get(uint32_t *tags, int p)
{
    int w;
    w = 0;
    if (p >= 32) {
       w = 1;
       p -= 32;
    }
    return (tags[w] >> p) & 1;
}

static int availf_nonempty(uint32_t *avail, int k)
{
    return availf_get(avail, k) != LOC_AVAIL(k);
}

static uint32_t* get_word(uint32_t *mem, uint16_t p_top, int p)
{
    uint32_t *avail;
    uint32_t *block;
    uint32_t p_avail;
    uint32_t p_block;

    p_avail = AVAIL(mem, p_top); 
    p_block = BLOCK(mem, p_top);

    avail = &mem[p_avail];
    block = &mem[p_block];
    if (p < 64) return &block[p];
    else return &avail[p - 64];
}

void blocklist_init(uint32_t *mem, uint16_t list)
{
    uint32_t *lp;
    uint16_t head;
    int i;
    lp = &mem[list];

    head = 0;

    for (i = 0; i < 1024; i++) {
        uint16_t node_addr;
        node_addr = list + i + 1;
        lp[node_addr] = head;
        head = node_addr;
    }

    lp[0] = head;

    /* TODO: first 16 blocks should be removed from list,
     * as they contain the data for the block list */
}

uint16_t blocklist_pop(uint32_t *mem, uint16_t list)
{
    uint32_t *lp;
    uint16_t head;
    uint16_t block;
    lp = &mem[list];

    head = lp[0];
    block = 0;

    if (head > 0) {
        uint16_t prev;
        prev = head;

        /* head = node.next */
        head = lp[head];

        /* update new head */
        lp[0] = head;

        /* prev.next = null */
        lp[prev] = 0;

        /* compute block number based on offset from head of list */
        block = prev - list;
    }


    return block;
}

void blocklist_push(uint32_t *mem, uint16_t list, uint16_t block)
{
    uint16_t node_addr;
    uint16_t head;
    uint32_t *lp;

    lp = &mem[list];

    /* get node address as memory offset based on block */
    node_addr = list + block;

    head = lp[0];

    /* node.next = head */
    lp[node_addr] = head;

    /* head = node */
    head = node_addr;

    /* update head */
    lp[0] = head;
}

/* bitset: 1024 bitmap used for cache */
void bitset_init(uint32_t *mem, uint16_t bm)
{
    uint16_t i;

    /* 1024 bits requires 32 words (half a block) */
    for (i = 0; i < 32; i++) {
        mem[i] = 0;
    }
}

/* add value to the set */
void bitset_add(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* ignore out of range values */
    if (val >= 1024) return;

    /* calculate word and bit positions */
    wpos = val / 32;
    bit = val % 32;

    mem[bm + wpos] |= 1 << bit;
}

/* remove value from set */
void bitset_remove(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* similar to add, but with AND logic */
    if (val >= 1024) return;

    wpos = val / 32;
    bit = val % 32;

    mem[bm + wpos] &= ~(1 << bit);
}

/* check to see if value exists in set */
int bitset_exists(uint32_t *mem, uint16_t bm, uint16_t val)
{
    uint8_t wpos;
    uint8_t bit;

    /* word/bit calculations */
    wpos = val / 32;
    bit = val % 32;
    return (mem[bm + wpos] & (1 << bit)) > 0;
}

void mem_init(uint32_t *mem,
              uint16_t p_top,
              uint16_t p_block,
              uint16_t p_avail,
              uint16_t p_tags)
{
    int i;
    int m;
    uint32_t *tags;
    uint32_t *avail;
    uint32_t *w;
    uint32_t *block;

    /* write virutal pointer addresses to memory */
    mem[p_top] = p_avail  | (p_block << 16);
    mem[p_top + 1] = p_tags;

    /* convert virtual pointers to memory addresses */
    block = &mem[p_block];
    avail = &mem[p_avail];
    tags = &mem[p_tags];

    m = 6;
    for (i = 0; i < 64; i++) block[i] = 0;
    for (i = 0; i < 7; i++) avail[i] = 0;

    tags[0] = tags[1] = 0;


    /* AVAILF[m] = AVAILB[m] = 0 */
    availf_set(avail, m, 0);
    availb_set(avail, m, 0);

    /* TAG(0) = 1 */
    tag_set(tags, 0, 1);
    /* KVAL(0) = m */
    kval_set(mem, p_top, 0, m);
    /* LINKF(0) = LINKB(0) = LOC(AVAIL[m]) */
    w = &block[0];
    *w = linkf_set(*w, LOC_AVAIL(m));
    *w = linkb_set(*w, LOC_AVAIL(m));

    /* AVAILF[k] = AVAILB[k] = LOC(AVAIL[k])*/
    for (i = 0; i < m; i++) {
        availf_set(avail, i, LOC_AVAIL(i));
        availb_set(avail, i, LOC_AVAIL(i));
    }
}

/* Reserve Block. TAOCP 2.5 "Buddy System" */
int mem_alloc(uint32_t *mem, uint16_t p_top, uint16_t k)
{
    int j, i;
    int L, P;
    uint32_t *tags;
    uint32_t *w;
    uint32_t *block;
    uint16_t p_tags;
    uint32_t *avail;

    p_tags = TAGS(mem, p_top);

    avail = get_avail(mem, p_top);
    tags = &mem[p_tags];
    block = &mem[BLOCK(mem, p_top)];
    /* R1: find block */

    j = -1;
    for (i = k; i <= 6; i++) {
        /* AVAILF != LOC(AVAIL(j) */
        if (availf_nonempty(avail, i)) {
            j = i;
            break;
        }
    }

    if (j < 0) {
        return -1;
    }

    /* R2: Remove from list */
    /* L <- AVAILB[j] */
    L = availb_get(avail, j);
    /* P <- LINKB(L) */
    P = linkb_get(block[L]);
    /* AVAILB[j] <- P */
    availb_set(avail, j, P);
    /* LINKF(P) <- LOC(AVAIL[j]) */
    w = get_word(mem, p_top, P);
    *w = linkf_set(*w, LOC_AVAIL(j));
    /* TAG(L) <- 0 */
    tag_set(tags, L, 0);

    /* R3: Split required? */
    while (j > k) {
        /* R4: Split */
        /* Decrease j by 1 */
        j--;
        /* P = L + 2^j */
        P = L + (1 << j);

        /* TAG(P) <- 1 */
        tag_set(tags, P, 1);
        /* KVAL(P) <- j */
        kval_set(mem, p_top, P, j);

        /* LINKF(P) <- LINKB(P) <- LOC(AVAIL[j]) */
        w = get_word(mem, p_top, P);
        *w = linkf_set(*w, LOC_AVAIL(j));
        *w = linkb_set(*w, LOC_AVAIL(j));

        /* AVAILF[j] <- AVAILB[j] <- P */
        availf_set(avail, j, P);
        availb_set(avail, j, P);
    }

    return L;
}

uint32_t mem_cksum(uint32_t *mem, uint16_t p_top)
{
    int L;
    int k;
    int navail;
    int nwords;
    uint32_t cksum;
    int b;
    uint32_t *block;
    uint32_t *avail;

    nwords = 0;
    cksum = 0;
    avail = get_avail(mem, p_top);

    k = 6;
    b = 0;
    block = &mem[BLOCK(mem, p_top)];

    for (k = 0; k <= 6; k++) {
        L = availf_get(avail, k);
        navail = 0;

        while (L != LOC_AVAIL(k)) {
            navail++;
            L = linkf_get(block[L]);
        }
        nwords += navail * (1 << k);
        cksum &= ~(7 << b);
        cksum |= (navail & 7) << b;
        b += 3;
    }

    cksum &= ~(127<< b);
    cksum |= (nwords & 127) << b;

    return cksum;
}

static int find_buddy(int x, int k)
{
    int xmod;
    xmod = x % (1 << (k + 1));
    if (xmod == 0) return x + (1 << k);
    return x - (1 << k);
}

/* adapted from "liberate" function */
void mem_free(uint32_t *mem, uint16_t p_top, int L, int k)
{
    int P;
    int m;
    uint32_t *tags;
    uint32_t *w, *q;
    uint16_t p_tags;
    uint32_t *avail;

    p_tags = TAGS(mem, p_top);
    m = 6;
    tags = &mem[p_tags];
    avail = get_avail(mem, p_top);

    /* S1: is buddy available? */
    while (1) {
        /* set P <- buddy_k(L) */
        P = find_buddy(L, k);
        /* if k = m or if TAG(P) = 0, or if TAG(P) = 1 and KVAL(P) != k), goto S3 */
        if (k == m) break;
        if (tag_get(tags, P) == 0) break;
        if (tag_get(tags, P) == 1 && kval_get(mem, p_top, P) != k) break;
        /* S2: combine with buddy */
        /* remove P from the AVAIL[k] list */
        /* LINKF(LINKB(P)) <- LINKF(P) */
        q = get_word(mem, p_top, P);
        w = get_word(mem, p_top, linkb_get(*q));
        *w = linkf_set(*w, linkf_get(*q));
        /* LINKB(LINKF(P)) <- LINKB(P) */
        w = get_word(mem, p_top, linkf_get(*q));
        *w = linkb_set(*w, linkb_get(*q));

        /* k <- k + 1 */
        k = k + 1;

        /* if P < L, set L <- P. return to S1 */
        if (P < L) {
            L = P;
        }
    }

    /* S3: put on list */
    /* TAG(L) <- 1 */
    tag_set(tags, L, 1);
    /* P <- AVAILF[k] */
    P = availf_get(avail, k);
    /* LINKF(L) <- P */
    w = get_word(mem, p_top, L);
    *w = linkf_set(*w, P);
    /* LINKB(P) <- L */
    w = get_word(mem, p_top, P);
    *w = linkb_set(*w, L);
    /* KVAL(L) <- k */
    kval_set(mem, p_top, L, k);
    /* LINKB(L) <- LOC(AVAIL[k]) */
    w = get_word(mem, p_top, L);
    *w = linkb_set(*w, LOC_AVAIL(k));
    /* AVAILF[k] <- L */
    availf_set(avail, k, L);
}

void bits_set(uint32_t *mem, uint32_t off, uint32_t sz, uint32_t w)
{
}

uint32_t bits_get(uint32_t *mem, uint32_t off, uint32_t sz)
{
    return 0;
}
