ugen_port ugen_port_constant(float c);
ugen_port ugen_port_from_word(uint32_t *mem, uint32_t w);
uint32_t ugen_port_to_word(uint32_t *mem, ugen_port *p);
float ugen_port_read(ugen_port *p, int n);
void ugen_port_write(ugen_port *p, int n, float s);
