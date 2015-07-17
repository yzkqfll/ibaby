/*
 * THER MTD
 *
 * Copyright (c) 2015 by Leo Liu <59089403@qq.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 *
 * 2015/06/01 - Init version
 *              by Leo Liu <59089403@qq.com>
 *
 */


#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_mtd.h"
#include "ther_misc.h"
#include "ther_spi_w25x40cl.h"

#define MODULE "[MTD    ] "

static struct mtd_info mtd;

struct mtd_info* get_mtd(void)
{
	return &mtd;
}

int8 ther_mtd_erase(struct mtd_info* m, uint32 addr, uint32 len, uint32 *ret_len)
{
	uint32 end_addr = addr + len;
	uint32 size_erased = 0;
	int8 ret;

	if ((addr & (m->erase_size - 1)) ||
		(len & (m->erase_size - 1))) {
		print(LOG_DBG, MODULE "ther_mtd_erase(): addr not aligned with 0x%x\n", m->erase_size);
		return -MTD_ERR_ADDR_NOT_ALIGN;
	}

	while (addr < end_addr) {
		ret = m->erase(addr, m->erase_size);
		if (ret)
			break;

		addr += m->erase_size;
		size_erased += m->erase_size;
	}

	if (ret_len)
		*ret_len = size_erased;

	return ret;
}

int8 ther_mtd_read(struct mtd_info* m, uint32 addr, void *buf, uint32 len, uint32 *ret_len)
{
	int8 ret = MTD_OK;
	uint32 size_read = 0;

	if (m->read_size == ULONG_MAX) {
		ret = m->read(addr, buf, len);
		if (!ret)
			size_read += len;
	}

	if (ret_len)
		*ret_len = size_read;

	return ret;
}

int8 ther_mtd_write(struct mtd_info* m, uint32 addr, void *buf, uint32 len, uint32 *ret_len)
{
	uint32 offset = 0;
	uint32 write_chunk;
	uint32 size_written = 0;
	int8 ret;

	while (offset < len) {
		if (len - offset >= m->write_size) {
			write_chunk = m->write_size;
		} else {
			write_chunk = len - offset;
		}

		ret = m->write(addr, (uint8 *)buf + offset, write_chunk);
		if (ret) {
			break;
		}
		size_written += write_chunk;

		offset += write_chunk;
	}

	if (ret_len)
		*ret_len = size_written;

	return ret;
}

int8 ther_mtd_open(struct mtd_info* m)
{
	return m->open();
}

int8 ther_mtd_close(struct mtd_info* m)
{
	return m->close();
}

void ther_mtd_init(void)
{
	struct mtd_info *m = &mtd;

	ther_spi_w25x_init(m);

	print(LOG_INFO, MODULE "chip size %lx, write_sz %lx, read_sz %lx, erase_sz %lx\n",
			m->size, m->write_size, m->read_size, m->erase_size);
}

