#define JI_BASE(X) (X & 0xFF)
#define JI_DEN(X) ((X >> 8) & 1023)
#define JI_NUM(X) ((X >> 18) & 1023)
#define JI_NEW(NN, NUM, DEN) ((NN) | ((DEN) << 8) | ((NUM) << 18))

uint32_t ji_new(uint8_t nn, uint16_t num, uint16_t den);
uint8_t ji_base(uint32_t j);
uint16_t ji_num(uint32_t j);
uint16_t ji_den(uint32_t j);
float ji_real(uint32_t j);
