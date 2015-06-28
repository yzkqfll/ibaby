/*
 * spi flash w25x device driver
 *
 * Change Log:
 * =======================
 * 6Jun15, yue.hu, created for w25x40cl
 */

#include "comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "ther_uart.h"
#include "ther_spi_drv.h"
#include "ther_spi_w25x40cl.h"


#define MODULE  "[THER W25X] "
#define W25X_DEBUG

#define CONFIG_W25X40CL

#define PAGE_SIZE            (256)

#ifdef CONFIG_W25X40CL
#define CHIP_SIZE           (512) //512KB

#define SECTOR_COUNT        (128)
#define BYTES_PER_SECTOR    (4096)     //4KB

#define BLOCK1_COUNT        (16)
#define BLOCK1_SIZE         (32*1024) //32KB
#define BLOCK2_COUNT        (8)
#define BLOCK2_SIZE         (64*1024) //64KB
#endif

/* JEDEC Manufacturers ID */
#define MF_ID           (0xEF)
#define GD_ID           (0xC8)

/* JEDEC Device ID: Memory type and Capacity */
#define MTC_W25X40CL    (0x3013)

/* command list */
#define CMD_WRSR                    (0x01)  /* Write Status Register */
#define CMD_PP                      (0x02)  /* Page Program */
#define CMD_READ                    (0x03)  /* Read Data */
#define CMD_WRDI                    (0x04)  /* Write Disable */
#define CMD_RDSR                    (0x05)  /* Read Status Register */
#define CMD_WREN                    (0x06)  /* Write Enable */
#define CMD_FAST_READ               (0x0B)  /* Fast Read */
#define CMD_ERASE_4K                (0x20)  /* Sector Erase:4K */
#define CMD_ERASE_32K               (0x52)  /* 32KB Block Erase */
#define CMD_ERASE_64K               (0xD8)  /* 64KB Block Erase */
#define CMD_JEDEC_ID                (0x9F)  /* Read JEDEC ID */
#define CMD_ERASE_CHIP              (0xC7)  /* Chip Erase */
#define CMD_RELEASE_PWRDN           (0xAB)  /* Release device from power down state */

#define DUMMY                       (0xFF)


static uint8 w25x_read_status(void)
{
	uint8 cmd = CMD_RDSR;
	uint8 value = 0;

	ther_spi_send_then_recv(&cmd, 1, &value, 1);

	return value;
}

static void w25x_wait_busy(void)
{
	while( w25x_read_status() & (0x01));
}

static void w25x_write_disable(void)
{
	uint8 cmd = CMD_WRDI;
	ther_spi_send(&cmd, 1);
}

static void w25x_write_enable(void)
{
	uint8 cmd = CMD_WREN;
	ther_spi_send(&cmd, 1);
}


/** \brief read [size] byte from [offset] to [buffer]
 *
 * \param offset uint32 unit : byte
 * \param buffer uint8*
 * \param size uint32   unit : byte
 * \return uint32 byte for read
 *
 */
static uint32 w25x_read_data(uint32 offset, uint8 *buffer, uint32 size)
{
	uint8 send_buffer[4];

	w25x_write_disable();

	send_buffer[0] = CMD_READ;
	send_buffer[1] = (uint8)(offset >> 16);
	send_buffer[2] = (uint8)(offset >> 8);
	send_buffer[3] = (uint8)(offset);

	ther_spi_send_then_recv(send_buffer, 4, buffer, size);

	return size;
}

/** \brief page program (<256 bytes) on [addr]
 *
 * \param addr uint32: byte
 * \param buffer const uint8*
 * \param size uint32 (1 - 256byte)
 * \return uint32
 *
 */
static uint32 w25x_page_program(uint32 addr, const uint8 *buffer, uint32 size)
{
	uint8 send_buffer[4];

	w25x_write_enable();

	send_buffer[0] = CMD_PP;
	send_buffer[1] = (uint8)(addr >> 16);
	send_buffer[2] = (uint8)(addr >> 8);
	send_buffer[3] = (uint8)(addr);

	ther_spi_send_then_send(send_buffer, 4, buffer, size);

	w25x_wait_busy();

	return size;
}


static bool w25x_sector_erase(uint32 sector_addr)
{
	uint8 send_buffer[4];

	w25x_write_enable();

	send_buffer[0] = CMD_ERASE_4K;
	send_buffer[1] = (sector_addr >> 16);
	send_buffer[2] = (sector_addr >> 8);
	send_buffer[3] = (sector_addr);
	ther_spi_send(send_buffer, 4);

	w25x_wait_busy(); // wait erase done.

	return FL_EOK;
}

static bool w25x_chip_erase(void)
{
	uint8 cmd;

	w25x_write_enable();

	cmd = CMD_ERASE_CHIP;
	ther_spi_send(&cmd, 1);

	w25x_wait_busy(); // wait erase done.

	return FL_EOK;
}

static bool w25x_flash_open(void)
{
	uint8 send_buffer[2];

	w25x_write_enable();

	send_buffer[0] = CMD_WRSR;
	send_buffer[1] = 0;
	ther_spi_send(send_buffer, 2);

	w25x_wait_busy();

	return FL_EOK;
}

static bool w25x_flash_close(void)
{
	return FL_EOK;
}

static uint32 w25x_flash_read(uint32 addr, void* buffer, uint32 size)
{
	w25x_read_data(addr, buffer, size);

	return size;
}

static uint32 w25x_flash_write(uint32 addr, const void* buffer, uint32 size)
{
	uint32 page_num  = size / PAGE_SIZE;
	const uint8* ptr = buffer;

	if(page_num == 0) {
		w25x_page_program(addr, ptr, size);
	} else {
		if ((addr & 0xFF) != 0) {
			print(LOG_ERR, MODULE "page addr must align to 256bytes,dead here!\n");
			while(1);
		}

		while(page_num--) {
			w25x_page_program(addr, buffer, 256);

			addr += 256;
			ptr += 256;
		}
	}

	w25x_write_disable();

	return size;
}

static bool w25x_flash_erase(uint32 addr, uint32 size)
{
	if(size == BYTES_PER_SECTOR)
		return w25x_sector_erase(addr);

	if(size == CHIP_SIZE)
		return w25x_chip_erase();

	return FL_ERR;
}

uint8 ther_spi_w25x_init(struct mtd_info *m)
{
	uint8 cmd;
	uint8 id_recv[3] = {0, 0, 0};
	uint16 memory_type_capacity;

	/* init spi */
	ther_spi_init();

	/* read flash id */
	cmd = CMD_JEDEC_ID;
	ther_spi_send_then_recv(&cmd, 1, id_recv, 3);

	if(id_recv[0] != MF_ID) {
		print(LOG_INFO, MODULE "Manufacturers ID(%x) error!\n", id_recv[0]);
		return FL_EID;
	}

	/* get the geometry information */
	m->size       = CHIP_SIZE;
	m->erase_size = BYTES_PER_SECTOR;
	m->write_size = PAGE_SIZE;

	/* get memory type and capacity */
	memory_type_capacity = id_recv[1];
	memory_type_capacity = (memory_type_capacity << 8) | id_recv[2];

	if(memory_type_capacity == MTC_W25X40CL) {
		print(LOG_INFO, MODULE "W25X40CL detection is ok\n");
	} else {
		print(LOG_INFO, MODULE "memory type(%x) capacity(%x) error!\n", id_recv[1], id_recv[2]);
		return FL_ETYPE;
	}

	/* callback */
	m->open    = w25x_flash_open;
	m->close   = w25x_flash_close;
	m->read    = w25x_flash_read;
	m->write   = w25x_flash_write;
	m->erase   = w25x_flash_erase;

	return FL_EOK;
}

#ifdef W25X_DEBUG
void ther_spi_w25x_test(uint32 sector_addr, uint32 offset, uint32 size)
{
	int i;
	uint8 *data_to_wr;
	uint8 *data_from_rd;

	print(LOG_INFO, MODULE "flash erase/write/verify test...");

	if(FL_EOK != w25x_flash_open()) {
		print(LOG_ERR, "open failed!\n");
		return;
	}

	data_to_wr = osal_mem_alloc(size);
	if(data_to_wr == NULL) {
		print(LOG_ERR, "out of memory!\n");
		return;
	}

	data_from_rd = osal_mem_alloc(size);
	if(data_from_rd == NULL) {
		print(LOG_ERR, "out of memory!\n");
		osal_mem_free(data_to_wr);
		return;
	}

	for(i = 0; i < size; i++) {
		data_to_wr[i] = 0x5a;
	}

	w25x_sector_erase(sector_addr);

	w25x_flash_write(sector_addr + offset, data_to_wr, size);

	w25x_flash_read(sector_addr + offset, data_from_rd, size);

	for(i = 0; i < size; i++) {
		if(data_from_rd[i] != data_to_wr[i]) {
			osal_mem_free(data_from_rd);
			osal_mem_free(data_to_wr);
			print(LOG_INFO, "failed(%d,%X)!\n", i, data_from_rd[i]);
			return;
		}
	}

	osal_mem_free(data_from_rd);
	osal_mem_free(data_to_wr);
	print(LOG_INFO, "OK!\n", i);
}
#endif


