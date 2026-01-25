#define MAX_INSTR 512

typedef uint32_t (*instr_func)(uint32_t *, uint16_t);

typedef struct {
    uint32_t key;
    instr_func func;
} instr_entry;

typedef struct {
    instr_entry ent[MAX_INSTR];
    uint32_t nent;
} instr_map;

void instr_map_init(instr_map *map);
void instr_map_set(instr_map *map, uint16_t key, instr_func func);
instr_func instr_map_get(instr_map *map, uint16_t key);
int instr_char_sym(char c);
int instr_sym_char(int sym);
uint16_t instr_key(const char *str);
