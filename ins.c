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
