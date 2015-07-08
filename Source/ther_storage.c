/*
 * THER STORAGE
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
#include "ther_storage.h"
#include "ther_mtd.h"
#include "ther_port.h"
#include "ther_misc.h"

#define MODULE "[THER STORAGE] "

enum {
	SECTOR_SYS = 0,

	SECTOR_ZERO_CAL,
	SECTOR_TEMP_CAL,

	SECTOR_HISTORY_MNGT,
	SECTOR_HISTORY_START,
	SECTOR_HISTORY_END = 126,

	SECTOR_TEST,
	SECTOR_NR = 128,
};

#define ERASED_UINT8 0xFF
#define ERASED_UINT16 0xFFFF

#define SYS_MAGIC 0x9527
#define STORAGE_FORMATED_FLAG 0x3333
struct sys_info {
	unsigned short magic;
	unsigned short storage_formated;
	unsigned short crc;
};

#define ZERO_CAL_MAGIC 0x1024
struct zero_cal {
	unsigned short magic;
    unsigned short hw_adc0;
    short delta;
    unsigned short crc;
};

#define TEMP_CAL_MAGIC 0xF00D
struct temp_cal {
	unsigned short magic;
	float R25_delta;
	float B_delta;
	unsigned short crc;
};

#define HIS_MNGT_MAGIC 0x1908
#define HIS_MNGT_UNIT_OBSOLETE 0xDEAD
struct his_mngt_unit {
	uint16 magic;
	uint8 start_sector;
	uint8 end_sector;
	uint16 status;
};

#define HIS_TEMP_MAGIC 0x0BED
struct his_temp_header {
	unsigned short magic;
	unsigned char valid;

	unsigned short crc;
};

#define BUF_LEN 256
struct ther_storage {
	uint32 his_mngt_addr;
	uint32 his_mngt_size;
	uint16 his_mngt_unit_size;

	uint8 history_start_sector;
	uint8 history_end_sector;

	uint8 buf[BUF_LEN];
};

static struct ther_storage ther_storage;

static bool is_clean(unsigned char *data, uint16 len)
{
	uint16 i;

	for (i = 0; i < len; i++) {
		if (data[i] != ERASED_UINT8)
			return FALSE;
	}

	return TRUE;
}

bool storage_write_zero_cal(unsigned short hw_adc0, short delta)
{
	struct mtd_info*m = get_mtd();
	int8 ret = TRUE;
	uint32 addr = SECTOR_ZERO_CAL * m->erase_size;
	struct zero_cal zc;

	if (ther_mtd_open(m))
		return FALSE;

	if (ther_mtd_erase(m, addr, m->erase_size, NULL)) {
		ther_mtd_close(m);
		return FALSE;
	}

	zc.hw_adc0 = hw_adc0;
	zc.delta = delta;

	if (ther_mtd_write(m, addr, &zc, sizeof(zc), NULL))
		ret = FALSE;

	ther_mtd_close(m);

	return ret;
}

bool storage_read_zero_cal(short *delta)
{
	struct mtd_info*m = get_mtd();
	int8 ret = TRUE;
	uint32 addr = SECTOR_ZERO_CAL * m->erase_size;
	struct zero_cal zc;

	if (ther_mtd_open(m))
		return FALSE;

	if (ther_mtd_read(m, addr, &zc, sizeof(zc), NULL)) {
		*delta = 0;
		ret = FALSE;
	} else {
		*delta = zc.delta;
	}

	ther_mtd_close(m);

	return ret;
}




static void encap_his_mngt_unit(struct his_mngt_unit *u, uint8 start, uint8 end)
{
	u->magic = HIS_MNGT_MAGIC;
	u->start_sector = start;
	u->end_sector = end;
	u->status = ERASED_UINT16;
}

static bool write_his_mngt_unit(struct ther_storage *ts, struct mtd_info*m,
								uint8 index, struct his_mngt_unit *u)
{
	uint32 addr = ts->his_mngt_addr + index * ts->his_mngt_unit_size;

	delay(UART_WAIT);
	print(LOG_ERR, MODULE "write his_mngt_unit([%d, %d]) at uint %d(addr 0x%lx)\n",
			u->start_sector, u->end_sector, index, addr);

	if (ther_mtd_write(m, addr, u, sizeof(struct his_mngt_unit), NULL)) {
		print(LOG_ERR, MODULE "fail to write his_mngt_unit at uint %d(addr 0x%lx)\n", index, addr);
		return FALSE;
	}
	delay(UART_WAIT);

	return TRUE;
}



static bool check_his_mngt_unit(struct ther_storage *ts, struct mtd_info*m, uint8 index)
{
	bool ret = FALSE;
	uint32 addr = ts->his_mngt_addr + index * ts->his_mngt_unit_size;

	if (ther_mtd_read(m, addr, ts->buf, BUF_LEN, NULL)) {
		print(LOG_ERR, MODULE "fail to read his_mngt_unit %d(addr 0xlx)\n", index, addr);
		return ret;
	}

	if (is_clean(ts->buf, BUF_LEN))
		ret = TRUE;

	return ret;
}

static bool storage_init_his_mngt(struct ther_storage *ts, struct mtd_info*m)
{
	bool ret = FALSE;
	unsigned char i;
	uint32 addr;
	unsigned char obsoleted = 0;
	struct his_mngt_unit unit;

	if (ther_mtd_open(m))
		goto out;

	ts->his_mngt_addr = m->erase_size * SECTOR_HISTORY_MNGT;
	ts->his_mngt_size = m->erase_size;
	ts->his_mngt_unit_size = m->write_size;
	print(LOG_DBG, MODULE "his_mngt_addr 0x%lx, his_mngt_size 0x%lx, his_mngt_unit_size 0x%x\n",
			ts->his_mngt_addr, ts->his_mngt_size, ts->his_mngt_unit_size);

	addr = ts->his_mngt_addr;
	for (i = 0; i < ts->his_mngt_size / ts->his_mngt_unit_size; i++) {

		if (ther_mtd_read(m, addr, &unit, sizeof(unit), NULL)) {
			print(LOG_ERR, MODULE "fail to read history_mngt_unit at addr 0x%x\n", addr);
			goto out;
		}

		if (unit.magic == HIS_MNGT_MAGIC) {
			if (unit.status != HIS_MNGT_UNIT_OBSOLETE)
				break;
			else
				obsoleted++;

		} else if (unit.magic == ERASED_UINT16) {
			if (i == 0) {
				print(LOG_ERR, MODULE "his_mngt area is clean\n");
				encap_his_mngt_unit(&unit, 0, 0);
				write_his_mngt_unit(ts, m, 0, &unit);
				break;
			} else {
				print(LOG_ERR, MODULE "unit %d(addr 0x%lx) is clean??\n", i, addr);
			}
		} else {
			print(LOG_ERR, MODULE "unknown magic(0x%x) at unit %d(addr 0x%lx)\n",
					unit.magic, i, addr);
		}

		addr += ts->his_mngt_unit_size;
	}

	if (i != ts->his_mngt_addr / ts->his_mngt_unit_size) {
		print(LOG_ERR, MODULE "find his_mngt_unit([%d, %d]) at unit %d(addr 0x%lx) \n",
				unit.start_sector, unit.end_sector, i, addr);
		ts->history_start_sector = unit.start_sector;
		ts->history_end_sector = unit.end_sector;

	} else {
		print(LOG_ERR, MODULE "do not find valid his_mngt_unit, erase his_mngt area\n");
		if (ther_mtd_erase(m, ts->his_mngt_addr, ts->his_mngt_size, NULL)) {
			print(LOG_ERR, MODULE "fail to erase his_mngt area at addr 0x%lx\n", addr);
			goto out;
		}
		encap_his_mngt_unit(&unit, 0, 0);
		write_his_mngt_unit(ts, m, 0, &unit);

		ts->history_start_sector = unit.start_sector;
		ts->history_end_sector = unit.end_sector;
	}

	ret = TRUE;

out:
	delay(UART_WAIT);
	ther_mtd_close(m);
	return ret;
}

bool storage_is_formated(void)
{
	struct mtd_info*m = get_mtd();
	struct sys_info info;
	bool ret = FALSE;
	uint32 addr = m->erase_size * SECTOR_SYS;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &info, sizeof(info), NULL)) {
		print(LOG_ERR, MODULE "fail to read sys_info at addr 0x%x\n", addr);
		goto out;
	}

	if ((info.magic == SYS_MAGIC) &&
		(info.storage_formated == STORAGE_FORMATED_FLAG)) {
		ret = TRUE;
	}

out:
	ther_mtd_close(m);

	return ret;
}

bool storage_format(void)
{
	struct mtd_info*m = get_mtd();
	struct sys_info info;
	bool ret = FALSE;
	uint32 addr = 0;

	if (ther_mtd_open(m))
		goto out;

	if (ther_mtd_erase(m, 0, m->size, NULL)) {
		goto out;
	}

	addr = m->erase_size * SECTOR_SYS;
	info.magic = SYS_MAGIC;
	info.storage_formated = STORAGE_FORMATED_FLAG;
	if (ther_mtd_write(m, addr, &info, sizeof(info), NULL)) {
		goto out;
	}

	ret = TRUE;

out:
	ther_mtd_close(m);

	return ret;
}

bool storage_erase(void)
{
	struct mtd_info*m = get_mtd();
	bool ret = FALSE;

	if (ther_mtd_open(m))
		goto out;

	if (ther_mtd_erase(m, 0, m->size, NULL)) {
		goto out;
	}

	ret = TRUE;

out:
	ther_mtd_close(m);

	return ret;
}

void ther_storage_init(void)
{
	struct mtd_info*m = get_mtd();
	struct ther_storage *ts = &ther_storage;

	if (!storage_is_formated()) {
		print(LOG_INFO, MODULE "storage is not formated, format it\n");
		if (storage_format())
			print(LOG_INFO, MODULE "storage format OK\n");
		else
			print(LOG_INFO, MODULE "fail to format storage\n");
	}

	if (!storage_init_his_mngt(ts, m))
		print(LOG_ERR, MODULE "fail to init history mngt\n");

}


bool ther_storage_test1(void)
{
	struct mtd_info*m = get_mtd();
	struct ther_storage *ts = &ther_storage;
	unsigned char i;
	uint32 addr;

	P0_6 = 1;
	if (ther_mtd_open(m))
		return FALSE;

	for (i = SECTOR_HISTORY_START; i <= SECTOR_HISTORY_END; i++) {
		addr = m->erase_size * i;
		print(LOG_DBG, "addr 0x%lx\n", addr);
		if (ther_mtd_read(m, addr, ts->buf, 13, NULL)) {
			return FALSE;
		}
	}

	ther_mtd_close(m);
	P0_6 = 0;

	return TRUE;
}

bool ther_storage_test(void)
{
	struct mtd_info*m = get_mtd();
	struct ther_storage *ts = &ther_storage;
	unsigned char i;
	uint32 addr;
	unsigned short b;

	P0_6 = 1;
	if (ther_mtd_open(m))
		return FALSE;

	for (i = SECTOR_HISTORY_START; i <= SECTOR_HISTORY_START; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_read(m, addr, (void *)&b, 2, NULL)) {
			return FALSE;
		}
		print(LOG_DBG, "read 0x%x\n", b);

		ts->buf[0] = 0x12;
		if (ther_mtd_write(m, addr, ts->buf, 1, NULL))
			return FALSE;

		if (ther_mtd_read(m, addr, (void *)&b, 2, NULL)) {
			return FALSE;
		}
		print(LOG_DBG, "read 0x%x\n", b);

		ts->buf[1] = 0x34;
		if (ther_mtd_write(m, addr + 1, ts->buf + 1, 1, NULL))
			return FALSE;

		if (ther_mtd_read(m, addr, (void *)&b, 2, NULL)) {
			return FALSE;
		}

		print(LOG_DBG, "read 0x%x\n", b);
	}

	ther_mtd_close(m);
	P0_6 = 0;

	return TRUE;
}
