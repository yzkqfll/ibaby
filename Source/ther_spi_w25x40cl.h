
#ifndef __THER_SPI_FLASH_H__
#define __THER_SPI_FLASH_H__

#define FL_EOK      0
#define FL_EID      1
#define FL_ETYPE    2

struct flash_device {

	uint32  sector_count;
	uint32  bytes_per_sector;
	uint32  page_size;

	uint8  (*init)   (void);
	uint8  (*open)   (void);
	uint8  (*close)  (void);
	uint32 (*read)  (int32 pos, void *buffer, uint32 size);
	uint32 (*write) (int32 pos, const void *buffer, uint32 size);
};

//#define ther_spi_flash_init() ther_spi_w25x_init()
uint8 ther_spi_w25x_init(void);

#endif

