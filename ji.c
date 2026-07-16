#include <stdint.h>
#include <math.h>

#include "ji.h"

uint32_t ji_new(uint8_t base, uint16_t num, uint16_t den)
{
    return JI_NEW(base, num, den);
}

uint8_t ji_base(uint32_t j)
{
    return JI_BASE(j);
}

uint16_t ji_num(uint32_t j)
{
    return JI_NUM(j);
}

uint16_t ji_den(uint32_t j)
{
    return JI_DEN(j);
}

float ji_real(uint32_t j)
{
    uint32_t num, den, base;
    float out;

    num = JI_NUM(j);
    den = JI_DEN(j);
    base = JI_BASE(j);

    out = pow(2, (base - 69.0) / 12.0) * 440;

    if (den != 0 && num != 0) {
        out *= (float)num/(float)den;
    }

    return out;
}
