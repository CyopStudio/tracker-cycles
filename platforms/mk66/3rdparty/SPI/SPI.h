#ifndef LIBS_SPI_H_
#define LIBS_SPI_H_

#include "fsl_common.h"

static inline uint8_t SPI_transfer(SPI_Type* base, uint8_t data)
{
    base->SR = SPI_SR_TCF_MASK;
    base->PUSHR = data;
    while (!(base->SR & SPI_SR_TCF_MASK))
        ;
    return base->POPR;
}

void SPI_init(SPI_Type* base);
void SPI_start(SPI_Type* base, uint32_t speed);
void SPI_end(SPI_Type* base);

#endif /* LIBS_SPI_H_ */
