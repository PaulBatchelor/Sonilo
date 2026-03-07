enum {
    PORT_NONE = 0,
    PORT_BLOCK,
    PORT_CONSTANT
};

typedef struct ugen_block {
    uint16_t addr;
    float *block;
} ugen_block;

typedef struct ugen_port {
    int type;
    union {
        float constant;
        ugen_block block;
    } data;
} ugen_port;

ugen_port ugen_port_constant(float c);
ugen_port ugen_port_from_word(uint32_t *mem, uint32_t w);
uint32_t ugen_port_to_word(uint32_t *mem, ugen_port *p);
float ugen_port_read(ugen_port *p, int n);
void ugen_port_write(ugen_port *p, int n, float s);
