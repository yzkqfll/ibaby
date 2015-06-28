
#ifndef __THER_SPI_FLASH_H__
#define __THER_SPI_FLASH_H__

#include "ther_mtd.h"

#define FL_ERR      0
#define FL_EOK      1
#define FL_EID      2
#define FL_ETYPE    3


//#define ther_spi_flash_init() ther_spi_w25x_init()
uint8 ther_spi_w25x_init(struct mtd_info *m);
void ther_spi_w25x_test(uint32 sector_addr, uint32 offset, uint32 size);

#endif

