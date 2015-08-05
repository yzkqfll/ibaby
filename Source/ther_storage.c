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
 * 2015/07/09 - Add history temp saving
 *              by Leo Liu <59089403@qq.com>
 *
 *
 */

#include <string.h>
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_storage.h"
#include "ther_mtd.h"
#include "ther_port.h"
#include "ther_misc.h"
#include "ther_data.h"
#include "ther_crc.h"

#define MODULE "[STORAGE] "

enum {
	SECTOR_SYS = 0,

	SECTOR_ZERO_CAL,
	SECTOR_TEMP_CAL,

	SECTOR_HISTORY_MNGT,
	SECTOR_HISTORY_START,
	SECTOR_HISTORY_END = 126, /* not included */

	SECTOR_TEST,
	SECTOR_NR = 128,
};

#define ERASED_UINT8 0xFF
#define ERASED_UINT16 0xFFFF

/*
 * system information
 */
#define SYS_MAGIC 0x9527
#define STORAGE_FORMATED_FLAG 0x3333
struct sys_info {
	unsigned short magic;
	unsigned short storage_formated;
	uint8 crc;
};

/*
 * calibration
 */
#define ZERO_CAL_MAGIC 0x1024
struct zero_cal {
	unsigned short magic;
    unsigned short hw_adc0;
    short delta;
    uint8 crc;
};

#define TEMP_CAL_MAGIC 0xF00D
struct temp_cal {
	unsigned short magic;

	float R_low;
	float t_low;
	float R_high;
	float t_high;

	float R25_delta;
	float B_delta;

	uint8 crc;
};

/*
 * history temp
 */
struct his_unit {
	uint16 magic;
	uint16 status;
};

#define HIS_MNGT_UNIT_MAGIC 0x1908
#define HIS_MNGT_UNIT_OBSOLETE 0xDEAD
struct his_mngt_unit {
	struct his_unit header;

	uint16 his_temp_head_unit;
	uint16 his_temp_tail_unit;
	uint8 crc;
};

#define HIS_TEMP_UNIT_MAGIC 0x1909
#define HIS_TEMP_UNIT_OBSOLETE 0xDEAD
struct his_temp_unit {
	struct his_unit header;

	uint16 data_len;
	uint8 crc; /* crc of data */

	/* temp info here */
};

enum {
	HIS_UNIT_STATUS_MTD_ERR,
	HIS_UNIT_STATUS_CLEAN,
	HIS_UNIT_STATUS_VALID,
	HIS_UNIT_STATUS_OBSOLETE,
	HIS_UNIT_STATUS_UNKNOWN,
};

enum {
	HIS_MNGT,
	HIS_TEMP,
};
static const char *his_type[] = {
	"his_mngt",
	"his_temp"
};


#define BUF_LEN 256 /* = unit size */

struct ther_storage {
	uint32 his_chunk_size; /* for erase */
	uint16 his_unit_size; /* for write */

	uint32 his_mngt_start_addr;
	uint32 his_mngt_end_addr;
	uint16 his_mngt_max_units;
	uint16 his_mngt_unit;

	uint32 his_temp_start_addr;
	uint32 his_temp_end_addr;
	uint16 his_temp_max_units;
	uint16 his_temp_head_unit;
	uint16 his_temp_tail_unit;

	uint8 temp_write_buf[BUF_LEN];
	uint16 offset;

	uint8 temp_read_buf[BUF_LEN];
};
static struct ther_storage *ther_storage;

#define OFFSET(s, m)   (uint32)(&(((s *)0)->m))

#define UNITS_PER_CHUNK(ts) ((ts)->his_chunk_size / (ts)->his_unit_size)

#define NEXT_UNIT(type, ts, unit) \
	((type == HIS_MNGT) ? (((unit) + 1) % (ts)->his_mngt_max_units) : \
			(((unit) + 1) % (ts)->his_temp_max_units))

#define NEXT_CHUNK(type, ts, unit) \
	(((type == HIS_MNGT) ? (((unit) + UNITS_PER_CHUNK(ts)) % (ts)->his_mngt_max_units) : \
			(((unit) + UNITS_PER_CHUNK(ts)) % (ts)->his_temp_max_units)) & (~(UNITS_PER_CHUNK(ts) - 1)))

#define HIS_ADDR(type, ts, unit) \
	((type == HIS_MNGT) ? ((ts)->his_mngt_start_addr + (unit) * (ts)->his_unit_size) : \
	((ts)->his_temp_start_addr + (unit) * (ts)->his_unit_size))


static void his_write_mngt_unit(struct ther_storage *ts, struct mtd_info *m, uint16 head_unit, uint16 tail_unit);

static struct ther_storage *get_ts(void)
{
	return ther_storage;
}

static void set_ts(struct ther_storage *ts)
{
	ther_storage = ts;
}

static bool is_clean(unsigned char *data, uint16 len)
{
	uint16 i;

	for (i = 0; i < len; i++) {
		if (data[i] != ERASED_UINT8)
			return FALSE;
	}

	return TRUE;
}

static uint16 his_get_temp_unit_nr(struct ther_storage *ts)
{
	uint16 head_unit, tail_unit;

	head_unit = ts->his_temp_head_unit;
	tail_unit = ts->his_temp_tail_unit;

	if (head_unit == tail_unit)
		return 0;
	else if (head_unit > tail_unit)
		return head_unit - tail_unit;
	else
		return head_unit + ts->his_temp_max_units - tail_unit;
}

static uint8 his_get_unit_status(struct ther_storage *ts, struct mtd_info *m, uint8 type, uint16 unit)
{
	uint8 ret;
	struct his_unit hu;
	uint16 expected_magic, obsoleted_status;
	uint32 addr = HIS_ADDR(type, ts, unit);

	if (type == HIS_MNGT) {
		expected_magic = HIS_MNGT_UNIT_MAGIC;
		obsoleted_status = HIS_MNGT_UNIT_OBSOLETE;

	} else if (type == HIS_TEMP) {
		expected_magic = HIS_TEMP_UNIT_MAGIC;
		obsoleted_status = HIS_TEMP_UNIT_OBSOLETE;
	}

	if (ther_mtd_open(m))
		return HIS_UNIT_STATUS_MTD_ERR;

	if (ther_mtd_read(m, addr, &hu, sizeof(hu), NULL)) {
		print(LOG_ERR, MODULE "his_get_unit_status(): fail to read %s unit %d(0x%lx)\n",
				his_type[type], unit, addr);
		ret = HIS_UNIT_STATUS_MTD_ERR;
		goto out;
	}

	if (hu.magic == ERASED_UINT16 && hu.status == ERASED_UINT16) {
		ret = HIS_UNIT_STATUS_CLEAN;

	} else if (hu.magic == expected_magic && hu.status == ERASED_UINT16) {
		ret = HIS_UNIT_STATUS_VALID;

	} else if (hu.magic == expected_magic && hu.status == obsoleted_status) {
		ret = HIS_UNIT_STATUS_OBSOLETE;

	} else {
		print(LOG_ERR, MODULE "his_get_unit_status(): %s unit %d(0x%lx) unknown(magic 0x%x, status 0x%x)\n",
				his_type[type], unit, addr, hu.magic, hu.status);
		ret = HIS_UNIT_STATUS_UNKNOWN;
	}

out:
	ther_mtd_close(m);

	return ret;
}

static bool his_read_unit(struct ther_storage *ts, struct mtd_info *m,
						uint8 type, uint16 unit, uint8 *buf, uint32 len)
{
	bool ret = FALSE;
	uint32 addr = HIS_ADDR(type, ts, unit);

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, buf, len, NULL)) {
		print(LOG_ERR, MODULE "fail to read %s unit %d(addr 0x%lx), len 0x%lx\n",
				his_type[type], unit, addr, len);
		goto out;
	}

	ret = TRUE;
out:
	ther_mtd_close(m);

	return ret;
}

static bool his_write_unit(struct ther_storage *ts, struct mtd_info *m,
						uint8 type, uint16 unit, uint8 *buf, uint32 len)
{
	bool ret = FALSE;
	uint32 addr = HIS_ADDR(type, ts, unit);

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_write(m, addr, buf, len, NULL)) {
		print(LOG_ERR, MODULE "fail to write %s unit %d(addr 0x%lx), len 0x%lx\n",
				his_type[type], unit, addr, len);
		goto out;
	}

	ret = TRUE;
out:
	ther_mtd_close(m);

	return ret;
}

static bool his_obsolete_unit(struct ther_storage *ts, struct mtd_info *m,
						uint8 type, uint16 unit, uint16 status)
{
	bool ret = FALSE;
	uint32 addr = HIS_ADDR(type, ts, unit);

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_write(m, addr + OFFSET(struct his_unit, status), (uint8 *)&status, sizeof(status), NULL)) {
		print(LOG_ERR, MODULE "fail to obsolete %s unit %d(0x%lx)\n",
				his_type[type], unit, addr);
		goto out;
	}

	ret = TRUE;
out:
	ther_mtd_close(m);

	return ret;
}

static bool his_erase_chunk(struct ther_storage *ts, struct mtd_info *m, uint8 type, uint16 unit)
{
	bool ret = FALSE;
	uint32 addr;

	if (unit & (UNITS_PER_CHUNK(ts) - 1)) {
		print(LOG_WARNING, MODULE "his_erase_chunk(): %s unit %d is not chunk aligned\n",
				his_type[type], unit);
		unit &= ~(UNITS_PER_CHUNK(ts) - 1);
	}
	addr = HIS_ADDR(type, ts, unit);

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_erase(m, addr, ts->his_chunk_size, NULL)) {
		print(LOG_ERR, MODULE "fail to erase %s chunk %d(addr 0x%lx)\n",
				his_type[type], unit, addr);
		goto out;
	}

	ret = TRUE;
out:
	ther_mtd_close(m);

	return ret;
}

static void __enqueue_temp_unit(struct ther_storage *ts)
{
	if (NEXT_UNIT(HIS_TEMP, ts, ts->his_temp_head_unit) == ts->his_temp_tail_unit) {
		/* full */
		ts->his_temp_tail_unit = NEXT_UNIT(HIS_TEMP, ts, ts->his_temp_tail_unit);
	}

	ts->his_temp_head_unit = NEXT_UNIT(HIS_TEMP, ts, ts->his_temp_head_unit);
}

static void __dequeue_temp_unit(struct ther_storage *ts)
{
	if (ts->his_temp_tail_unit == ts->his_temp_head_unit) {
		/* empty */
	} else {
		ts->his_temp_tail_unit = NEXT_UNIT(HIS_TEMP, ts, ts->his_temp_tail_unit);
	}
}

static void his_enqueue_temp_unit(struct ther_storage *ts, struct mtd_info *m, struct his_temp_unit *tu)
{
	uint8 status;

	status = his_get_unit_status(ts, m, HIS_TEMP, ts->his_temp_head_unit);

	switch (status) {
	case HIS_UNIT_STATUS_CLEAN:
		break;

	default:
		print(LOG_DBG, MODULE "his_temp unit %d is not clean(%d), erase the chunk\n",
				ts->his_temp_head_unit, status);
		his_erase_chunk(ts, m, HIS_TEMP, ts->his_temp_head_unit);
		/* move to next unit to avoid bad block */
//		__enqueue_temp_unit(ts);
		break;
	}

	print(LOG_DBG, MODULE "-> Enqueue his_temp to unit %d, len %d\n",
			ts->his_temp_head_unit, ts->offset);
	his_write_unit(ts, m, HIS_TEMP, ts->his_temp_head_unit, (uint8 *)tu, ts->offset);

	__enqueue_temp_unit(ts);
	his_write_mngt_unit(ts, m, ts->his_temp_head_unit, ts->his_temp_tail_unit);
}

static bool his_dequeue_temp_unit(struct ther_storage *ts, struct mtd_info *m, struct his_temp_unit **tu)
{
	bool ret = FALSE;

	switch (his_get_unit_status(ts, m, HIS_TEMP, ts->his_temp_tail_unit)) {
	case HIS_UNIT_STATUS_VALID:
		his_read_unit(ts, m, HIS_TEMP, ts->his_temp_tail_unit, ts->temp_read_buf, ts->his_unit_size);
		*tu = (struct his_temp_unit *)ts->temp_read_buf;
		print(LOG_DBG, MODULE "<- Dequeue his_temp from unit %d, len %d\n",
				ts->his_temp_tail_unit, (*tu)->data_len);

		his_obsolete_unit(ts, m, HIS_TEMP, ts->his_temp_tail_unit, HIS_TEMP_UNIT_OBSOLETE);
		ret = TRUE;
		break;

	default:
		break;
	}

	__dequeue_temp_unit(ts);
	his_write_mngt_unit(ts, m, ts->his_temp_head_unit, ts->his_temp_tail_unit);

	return ret;
}

static void his_encap_mngt_unit(struct his_mngt_unit *mu, uint16 head_unit, uint16 tail_unit)
{
	mu->header.magic = HIS_MNGT_UNIT_MAGIC;
	mu->header.status = ERASED_UINT16;
	mu->his_temp_head_unit = head_unit;
	mu->his_temp_tail_unit = tail_unit;
	mu->crc = crc7_be(0, (const uint8 *)mu, OFFSET(struct his_mngt_unit, crc));
}

static void his_write_mngt_unit(struct ther_storage *ts, struct mtd_info *m, uint16 head_unit, uint16 tail_unit)
{
	struct his_mngt_unit mu;
	uint8 status;

	/* mark current mngt unit obsolete */
	his_obsolete_unit(ts, m, HIS_MNGT, ts->his_mngt_unit, HIS_MNGT_UNIT_OBSOLETE);

	/* move to next unit(clean) */
	ts->his_mngt_unit = NEXT_UNIT(HIS_MNGT, ts, ts->his_mngt_unit);

	status = his_get_unit_status(ts, m, HIS_MNGT, ts->his_mngt_unit);
	switch (status) {
	case HIS_UNIT_STATUS_CLEAN:
		break;

	default:
		print(LOG_DBG, MODULE "his_mngt unit %d is not clean(%d), erase the chunk\n",
				ts->his_mngt_unit, status);
		his_erase_chunk(ts, m, HIS_MNGT, ts->his_mngt_unit);
		break;
	}

	his_encap_mngt_unit(&mu, head_unit, tail_unit);
	print(LOG_DBG, MODULE "write his_mngt unit %d: his_temp head at %d, tail at %d(%d)\n",
			ts->his_mngt_unit, head_unit, tail_unit, his_get_temp_unit_nr(ts));
	his_write_unit(ts, m, HIS_MNGT, ts->his_mngt_unit, (uint8 *)&mu, sizeof(mu));
}

static void his_write_temp_unit(struct ther_storage *ts, struct mtd_info *m)
{
	struct his_temp_unit *tu = (struct his_temp_unit *)ts->temp_write_buf;

	tu->header.magic = HIS_TEMP_UNIT_MAGIC;
	tu->header.status = ERASED_UINT16;
	tu->data_len = ts->offset - sizeof(struct his_temp_unit);
	tu->crc = crc7_be(0, (const uint8 *)(tu + 1), tu->data_len);

	if (tu->data_len) {
		his_enqueue_temp_unit(ts, m, tu);
		ts->offset = sizeof(struct his_temp_unit);
	}
}

static bool his_init_temp(struct ther_storage *ts, struct mtd_info*m)
{
	switch (his_get_unit_status(ts, m, HIS_TEMP, ts->his_temp_head_unit)) {
	case HIS_UNIT_STATUS_CLEAN:
		break;

	default:
		print(LOG_DBG, MODULE "his_init_temp(): head unit %d status %d\n",
				ts->his_temp_head_unit, his_get_unit_status(ts, m, HIS_TEMP, ts->his_temp_head_unit));
		his_erase_chunk(ts, m, HIS_TEMP, ts->his_temp_head_unit);
		__enqueue_temp_unit(ts); /* move to next unit to avoid bad block */
		his_write_mngt_unit(ts, m, ts->his_temp_head_unit, ts->his_temp_tail_unit);
		break;
	}

	return TRUE;
}

static void his_reset(struct ther_storage *ts, struct mtd_info*m)
{
	struct his_mngt_unit mu;
	uint16 unit;

	ts->his_mngt_unit = 0;

	ts->his_temp_head_unit = 0;
	ts->his_temp_tail_unit = 0;

	ts->offset = sizeof(struct his_temp_unit);

	/*
	 * his mngt
	 */
	for (unit = 0; unit < ts->his_mngt_max_units; unit += UNITS_PER_CHUNK(ts)) {
		his_erase_chunk(ts, m, HIS_MNGT, unit);
	}
	print(LOG_DBG, MODULE "reset his_mngt, head at %d\n", ts->his_mngt_unit);

	his_encap_mngt_unit(&mu, ts->his_temp_head_unit, ts->his_temp_tail_unit);
	his_write_unit(ts, m, HIS_MNGT, ts->his_mngt_unit, (uint8 *)&mu, sizeof(mu));

	/*
	 * his temp
	 */
	for (unit = 0; unit < ts->his_temp_max_units; unit += UNITS_PER_CHUNK(ts)) {
		his_erase_chunk(ts, m, HIS_TEMP, unit);
	}
	print(LOG_DBG, MODULE "reset his_temp: head at %d, tail at %d now, %d valid units\n",
			ts->his_temp_head_unit, ts->his_temp_tail_unit,
			his_get_temp_unit_nr(ts));
}

static bool his_init_mngt(struct ther_storage *ts, struct mtd_info*m)
{
	uint16 unit;
	unsigned char obsoleted = 0;
	struct his_mngt_unit mu;
	bool exit = FALSE;
	uint8 crc;

	print(LOG_ERR, MODULE "scan his_mngt area\n");

	for (unit = 0; unit < ts->his_mngt_max_units; unit++) {

		switch (his_get_unit_status(ts, m, HIS_MNGT, unit)) {
		case HIS_UNIT_STATUS_CLEAN:
			if (unit & ~(UNITS_PER_CHUNK(ts) - 1))
				print(LOG_DBG, MODULE, "his_init_mngt(): clean unit at %d not aligned??\n", unit);

			his_encap_mngt_unit(&mu, 0, 0);
			his_write_unit(ts, m, HIS_MNGT, 0, (uint8 *)&mu, sizeof(mu));
			exit = TRUE;
			break;

		case HIS_UNIT_STATUS_VALID:
			print(LOG_ERR, MODULE "find his_mngt head unit at %d\n", unit);
			ts->his_mngt_unit = unit;

			/* get his_temp head & tail unit */
			his_read_unit(ts, m, HIS_MNGT, ts->his_mngt_unit, (uint8 *)&mu, sizeof(mu));
			crc = crc7_be(0, (const uint8 *)&mu, OFFSET(struct his_mngt_unit, crc));
			if (crc != mu.crc) {
				print(LOG_DBG, MODULE "his_mngt_unit crc error: calculated 0x%x, saved 0x%x\n",
												crc, mu.crc);
				break;
			}

			ts->his_temp_head_unit = mu.his_temp_head_unit;
			ts->his_temp_tail_unit = mu.his_temp_tail_unit;

			print(LOG_ERR, MODULE "find his_temp head at %d, tail at %d, %d valid units\n",
					ts->his_temp_head_unit, ts->his_temp_tail_unit,
					his_get_temp_unit_nr(ts));

			exit = TRUE;
			break;

		case HIS_UNIT_STATUS_OBSOLETE:
//			print(LOG_ERR, MODULE "obsoleted his_mngt unit %d\n", unit);
			obsoleted++;
			break;

		default:
			print(LOG_ERR, MODULE "his_mngt unit %d status: %d\n",
					unit, his_get_unit_status(ts, m, HIS_MNGT, unit));
			break;
		}

		if (exit)
			break;
	}

	if (unit == ts->his_mngt_max_units) {
		his_reset(ts, m);
	}

	delay(UART_WAIT);

	return TRUE;
}

static void his_init_patition(struct ther_storage *ts, struct mtd_info*m)
{
	ts->his_chunk_size = m->erase_size;
	ts->his_unit_size = m->write_size;

	/* mngt */
	ts->his_mngt_start_addr = m->erase_size * SECTOR_HISTORY_MNGT;
	ts->his_mngt_end_addr = ts->his_mngt_start_addr + m->erase_size;
	ts->his_mngt_max_units = (ts->his_mngt_end_addr - ts->his_mngt_start_addr) / ts->his_unit_size;
	ts->his_mngt_unit = 0;

	/* temp */
	ts->his_temp_start_addr = m->erase_size * SECTOR_HISTORY_START;
	ts->his_temp_end_addr = m->erase_size * SECTOR_HISTORY_END;
	ts->his_temp_max_units = (ts->his_temp_end_addr - ts->his_temp_start_addr) / ts->his_unit_size;
	ts->his_temp_head_unit = 0;
	ts->his_temp_tail_unit = 0;

	ts->offset = sizeof(struct his_temp_unit);
}


void ther_storage_init(void)
{
	struct mtd_info *m = get_mtd();
	struct ther_storage *ts;

	ts = osal_mem_alloc(sizeof(struct ther_storage));
	if (!ts) {
		print(LOG_ERR, MODULE "fail to alloc struct ther_storage\n");
		return;
	}
	set_ts(ts);
	memset(ts, 0, sizeof(struct ther_storage));

	if (!storage_is_formated()) {
		uart_delay(UART_WAIT);
		print(LOG_INFO, MODULE "storage is not formated, format it\n");
		if (!storage_format())
			print(LOG_INFO, MODULE "fail to format storage\n");
	}

	his_init_patition(ts, m);

	if (!his_init_mngt(ts, m))
		print(LOG_ERR, MODULE "fail to init his mngt\n");

	if (!his_init_temp(ts, m))
		print(LOG_ERR, MODULE "fail to init his temp\n");
}



void ther_storage_exit(void)
{
	struct ther_storage *ts = get_ts();

	if (ts)
		osal_mem_free(ts);
}


void storage_reset(void)
{
	struct ther_storage *ts = get_ts();
	struct mtd_info*m = get_mtd();

	his_reset(ts, m);
}

void storage_drain_temp(void)
{
	struct ther_storage *ts = get_ts();
	struct mtd_info*m = get_mtd();

	print(LOG_DBG, MODULE "drain out his_temp unit to head %d\n", ts->his_temp_head_unit);
	his_write_temp_unit(ts, m);
}

bool storage_save_temp(uint8 *data, uint16 len)
{
	struct ther_storage *ts = get_ts();
	struct mtd_info*m = get_mtd();
	struct temp_data *td = (struct temp_data *)data;
	bool ret = TRUE;

	if (ts->offset + len >= BUF_LEN) {
		his_write_temp_unit(ts, m);
	}

	print(LOG_DBG, MODULE "Store: %d-%02d-%02d %02d:%02d:%02d, temp %ld, (at offset %d, len %d)\n",
				td->year, td->month, td->day, td->hour, td->minutes, td->seconds,
				td->temp & 0xFFFFFF,
				ts->offset, len);
	memcpy(ts->temp_write_buf + ts->offset, data, len);
	ts->offset += len;

	return ret;
}

void storage_restore_temp(uint8 **buf, uint16 *len)
{
	struct ther_storage *ts = get_ts();
	struct mtd_info *m = get_mtd();
	struct his_temp_unit *tu;
	uint8 crc;

	*buf = NULL;
	*len = 0;
	while (his_get_temp_unit_nr(ts) > 0) {
		if (his_dequeue_temp_unit(ts, m, &tu)) {
			crc = crc7_be(0, (const uint8 *)(tu + 1), tu->data_len);
			if (tu->crc != crc) {
				print(LOG_DBG, MODULE "temp crc error: calculated 0x%x, saved 0x%x\n",
						crc, tu->crc);
				continue;
			}
			*buf = (uint8 *)tu + sizeof(struct his_temp_unit);;
			*len = tu->data_len;
			break;
		}
	};

}


void storage_show_info(void)
{
	struct ther_storage *ts = get_ts();
	struct mtd_info*m = get_mtd();
	struct his_mngt_unit mu;

	print(LOG_DBG, MODULE "\n");

	print(LOG_DBG, MODULE "his chunksize 0x%lx, unitsize 0x%x\n",
			ts->his_chunk_size, ts->his_unit_size);

	print(LOG_DBG, MODULE "his_mngt area [0x%lx, 0x%lx), %d units\n",
			ts->his_mngt_start_addr, ts->his_mngt_end_addr,
			ts->his_mngt_max_units);

	print(LOG_DBG, MODULE "his_temp area [0x%lx, 0x%lx), %d units\n",
			ts->his_temp_start_addr, ts->his_temp_end_addr,
			ts->his_temp_max_units);

	/* get his_temp head & tail unit */
	his_read_unit(ts, m, HIS_MNGT, ts->his_mngt_unit, (uint8 *)&mu, sizeof(mu));
	print(LOG_ERR, MODULE "his_mngt unit head %d(0x%lx): his_temp head at %d, tail at %d\n",
			ts->his_mngt_unit, HIS_ADDR(HIS_MNGT, ts, ts->his_mngt_unit),
			mu.his_temp_head_unit, mu.his_temp_tail_unit);

	print(LOG_ERR, MODULE "his_temp unit head %d(0x%lx), tail %d(0x%lx), %d valid units\n",
			ts->his_temp_head_unit, HIS_ADDR(HIS_TEMP, ts, ts->his_temp_head_unit),
			ts->his_temp_tail_unit, HIS_ADDR(HIS_TEMP, ts, ts->his_temp_tail_unit),
			his_get_temp_unit_nr(ts));

	print(LOG_DBG, MODULE "\n");
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
	zc.crc = crc7_be(0, (const uint8 *)&zc, OFFSET(struct zero_cal, crc));

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
	uint8 crc;

	if (ther_mtd_open(m))
		return FALSE;

	if (ther_mtd_read(m, addr, &zc, sizeof(zc), NULL)) {
		*delta = 0;
		ret = FALSE;
	} else {

		crc = crc7_be(0, (const uint8 *)&zc, OFFSET(struct zero_cal, crc));
		if (crc == zc.crc)
			*delta = zc.delta;
		else
			*delta = 0;
	}

	ther_mtd_close(m);

	return ret;
}

bool storage_write_low_temp_cal(float R_low, float t_low)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		goto out;
	}

	if (ther_mtd_erase(m, addr, m->erase_size, NULL)) {
		goto out;
	}

	tc.magic = ZERO_CAL_MAGIC;
	tc.R_low = R_low;
	tc.t_low = t_low;
	tc.crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));

	if (ther_mtd_write(m, addr, &tc, sizeof(tc), NULL))
		goto out;

	ret = TRUE;
out:
	ther_mtd_close(m);
	return ret;
}

bool storage_read_low_temp_cal(float *R_low, float *t_low)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;
	uint8 crc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		*R_low = 0;
		*t_low = 0;
		goto out;
	} else {

		crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));
		if (crc == tc.crc) {
			*R_low = tc.R_low;
			*t_low = tc.t_low;

			ret = TRUE;
		} else {
			*R_low = 0;
			*t_low = 0;
		}
	}
	ret = TRUE;

out:
	ther_mtd_close(m);
	return ret;
}

bool storage_write_high_temp_cal(float R_high, float t_high)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		goto out;
	}

	if (ther_mtd_erase(m, addr, m->erase_size, NULL)) {
		goto out;
	}

	tc.magic = ZERO_CAL_MAGIC;
	tc.R_high = R_high;
	tc.t_high = t_high;
	tc.crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));

	if (ther_mtd_write(m, addr, &tc, sizeof(tc), NULL))
		goto out;

	ret = TRUE;
out:
	ther_mtd_close(m);
	return ret;
}

bool storage_read_high_temp_cal(float *R_high, float *t_high)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;
	uint8 crc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		*R_high = 0;
		*t_high = 0;
		goto out;
	} else {

		crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));
		if (crc == tc.crc) {
			*R_high = tc.R_high;
			*t_high = tc.t_high;

			ret = TRUE;
		} else {
			*R_high = 0;
			*t_high = 0;
		}
	}
	ret = TRUE;

out:
	ther_mtd_close(m);
	return ret;
}

bool storage_write_temp_cal(float B_delta, float R25_delta)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		goto out;
	}

	if (ther_mtd_erase(m, addr, m->erase_size, NULL)) {
		goto out;
	}

	tc.magic = ZERO_CAL_MAGIC;
	tc.B_delta = B_delta;
	tc.R25_delta = R25_delta;
	tc.crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));

	if (ther_mtd_write(m, addr, &tc, sizeof(tc), NULL))
		goto out;

	ret = TRUE;

out:
	ther_mtd_close(m);
	return ret;
}

bool storage_read_temp_cal(float *B_delta, float *R25_delta)
{
	struct mtd_info*m = get_mtd();
	int8 ret = FALSE;
	uint32 addr = SECTOR_TEMP_CAL * m->erase_size;
	struct temp_cal tc;
	uint8 crc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &tc, sizeof(tc), NULL)) {
		*B_delta = 0;
		*R25_delta = 0;
		goto out;
	} else {

		crc = crc7_be(0, (const uint8 *)&tc, OFFSET(struct temp_cal, crc));
		if (crc == tc.crc) {
			*B_delta = tc.B_delta;
			*R25_delta = tc.R25_delta;
			ret = TRUE;
		} else {
			*B_delta = 0;
			*R25_delta = 0;
		}
	}
	ret = TRUE;

out:
	ther_mtd_close(m);
	return ret;
}

bool storage_is_formated(void)
{
	struct mtd_info*m = get_mtd();
	struct sys_info info;
	bool ret = FALSE;
	uint32 addr = m->erase_size * SECTOR_SYS;
	uint8 crc;

	if (ther_mtd_open(m))
		return ret;

	if (ther_mtd_read(m, addr, &info, sizeof(info), NULL)) {
		print(LOG_ERR, MODULE "fail to read sys_info at addr 0x%x\n", addr);
		goto out;
	}

	crc = crc7_be(0, (const uint8 *)&info, OFFSET(struct sys_info, crc));
	if (info.crc != crc) {
		print(LOG_DBG, MODULE "sys_info crc error: calculated 0x%x, saved 0x%x\n",
								crc, info.crc);
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
	info.crc = crc7_be(0, (const uint8 *)&info, OFFSET(struct sys_info, crc));

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

bool storage_test(void)
{
	struct mtd_info*m = get_mtd();
	struct ther_storage *ts = get_ts();
	uint16 i, j;
	uint8 data;
	uint32 addr;
	bool ret = FALSE;

	if (ther_mtd_open(m))
		return FALSE;

	print(LOG_DBG, MODULE "1. SPI sanity test\n");
	print(LOG_DBG, MODULE "   erase sector -> write 0x00 -> check sector\n");
	data = 0x0;
	osal_memset(ts->temp_write_buf, data, BUF_LEN);
	for (i = SECTOR_SYS; i < SECTOR_NR; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_write(m, addr, ts->temp_write_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   write sector %d failed\n", i);
			goto out;
		}

		if (ther_mtd_read(m, addr, ts->temp_read_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   read sector %d failed\n", i);
			goto out;
		}

		for (j = 0; j < BUF_LEN; j++) {
			if (ts->temp_read_buf[j] != data) {
				print(LOG_DBG, MODULE "   Err: sector %d, offset %d is 0x%x(expected 0x%x)\n",
						i, j, ts->temp_read_buf[i], data);
				goto out;
			}
		}
	}

	print(LOG_DBG, MODULE "2. SPI Regression test\n");

	print(LOG_DBG, MODULE "   erase sector -> write 0xaa -> check sector\n");
	data = 0xaa;
	osal_memset(ts->temp_write_buf, data, BUF_LEN);
	for (i = SECTOR_SYS; i < SECTOR_NR; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_write(m, addr, ts->temp_write_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   write sector %d failed\n", i);
			goto out;
		}

		if (ther_mtd_read(m, addr, ts->temp_read_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   read sector %d failed\n", i);
			goto out;
		}

		for (j = 0; j < BUF_LEN; j++) {
			if (ts->temp_read_buf[j] != data) {
				print(LOG_DBG, MODULE "   Err: sector %d, offset %d is 0x%x(expected 0x%x)\n",
						i, j, ts->temp_read_buf[i], data);
				goto out;
			}
		}
	}

	print(LOG_DBG, MODULE "   erase sector -> write 0x55 -> check sector\n");
	data = 0x55;
	osal_memset(ts->temp_write_buf, data, BUF_LEN);
	for (i = SECTOR_SYS; i < SECTOR_NR; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_write(m, addr, ts->temp_write_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   write sector %d failed\n", i);
			goto out;
		}

		if (ther_mtd_read(m, addr, ts->temp_read_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   read sector %d failed\n", i);
			goto out;
		}

		for (j = 0; j < BUF_LEN; j++) {
			if (ts->temp_read_buf[j] != data) {
				print(LOG_DBG, MODULE "   Err: sector %d, offset %d is 0x%x(expected 0x%x)\n",
						i, j, ts->temp_read_buf[i], data);
				goto out;
			}
		}
	}

	print(LOG_DBG, MODULE "   erase sector -> write 0x5a -> check sector\n");
	data = 0x5a;
	osal_memset(ts->temp_write_buf, data, BUF_LEN);
	for (i = SECTOR_SYS; i < SECTOR_NR; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_write(m, addr, ts->temp_write_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   write sector %d failed\n", i);
			goto out;
		}

		if (ther_mtd_read(m, addr, ts->temp_read_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   read sector %d failed\n", i);
			goto out;
		}

		for (j = 0; j < BUF_LEN; j++) {
			if (ts->temp_read_buf[j] != data) {
				print(LOG_DBG, MODULE "   Err: sector %d, offset %d is 0x%x(expected 0x%x)\n",
						i, j, ts->temp_read_buf[i], data);
				goto out;
			}
		}
	}

	print(LOG_DBG, MODULE "   erase sector -> write 0xa5 -> check sector\n");
	data = 0xa5;
	osal_memset(ts->temp_write_buf, data, BUF_LEN);
	for (i = SECTOR_SYS; i < SECTOR_NR; i++) {
		addr = m->erase_size * i;

		ther_mtd_erase(m, addr, m->erase_size, NULL);

		if (ther_mtd_write(m, addr, ts->temp_write_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   write sector %d failed\n", i);
			goto out;
		}

		if (ther_mtd_read(m, addr, ts->temp_read_buf, BUF_LEN, NULL)) {
			print(LOG_DBG, MODULE "   read sector %d failed\n", i);
			goto out;
		}

		for (j = 0; j < BUF_LEN; j++) {
			if (ts->temp_read_buf[j] != data) {
				print(LOG_DBG, MODULE "   Err: sector %d, offset %d is 0x%x(expected 0x%x)\n",
						i, j, ts->temp_read_buf[i], data);
				goto out;
			}
		}
	}
	ret = TRUE;

out:
	ther_mtd_close(m);

	print(LOG_INFO, MODULE "Format SPI storage\n");
	if (!storage_format())
		print(LOG_INFO, MODULE "fail to format storage\n");

	if(ret)
		print(LOG_DBG, MODULE "OK\n");
	else
		print(LOG_DBG, MODULE "ERROR\n");

	return ret;
}
