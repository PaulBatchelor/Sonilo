#include <stdlib.h>
#include <stdint.h>
#include "ins.h"

void instr_map_init(instr_map *map)
{
    int i;
    for (i = 0; i < MAX_INSTR; i++) {
        map->ent[i].key = 0;
        map->ent[i].func = NULL;
    }
}

static uint16_t hash(uint16_t key) {
    /* TODO: make this hash better */
    return key % MAX_INSTR;
}

void instr_map_set(instr_map *map, uint16_t key, instr_func func)
{
    uint16_t h, i;

    h = hash(key);
    for (i = 0; i < MAX_INSTR; i++) {
        instr_entry *ent;
        ent = &map->ent[h];
        if (ent->func == NULL) {
            ent->key = key;
            ent->func = func;
            break;
        }
        h += 1;
        h %= MAX_INSTR;
    }
}

instr_func instr_map_get(instr_map *map, uint16_t key)
{
    uint16_t h, i;
    h = hash(key);

    for (i = 0; i < MAX_INSTR; i++) {
        instr_entry *ent;
        ent = &map->ent[h];
        if (ent->func != NULL && ent->key == key) {
            return ent->func;
        }
        h += 1;
        h %= MAX_INSTR;
    }

    return NULL;
}

/* convert ASCII character into 5-bit encoding scheme */
int instr_char_sym(char c)
{
    /* case insensitive a-zA-Z mapping */
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}

/* convert symbol into ascii character */
static const char *lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!*+-/=";
int instr_sym_char(int sym)
{
    if (sym < 0 || sym >= 32) return -1;
    return lookup[sym];
}
