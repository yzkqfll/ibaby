
#ifndef __THER_SPI_FLASH_H__
#define __THER_SPI_FLASH_H__

#include "ther_mtd.h"

int8 ther_spi_w25x_init(struct mtd_info *m);
void ther_spi_w25x_test(uint32 sector_addr, uint32 offset, uint32 size);

#endif

