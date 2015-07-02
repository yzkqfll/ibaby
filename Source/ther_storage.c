
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_storage.h"
#include "ther_mtd.h"

#define MODULE "[THER STORAGE] "

enum {
	SECTOR_ZERO_CAL = 0,
	SECTOR_HIGH_TEMP_CAL,
	SECTOR_LOW_TEMP_CAL,
};

struct zero_cal {
    unsigned short hw_adc0;
    short delta;
};

bool ther_write_zero_cal_info(unsigned short hw_adc0, short delta)
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

bool ther_read_zero_cal_info(short *delta)
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

void ther_storage_test(void)
{
//	struct mtd_info*m = get_mtd();
//	short delta;
//	unsigned short data = 0x1234, data2;
//	struct zero_cal zc, zc2;

//	if (!ther_write_zero_cal_info(66, 100))
//		print(LOG_DBG, "fail to write\n");
//
//	if (!ther_read_zero_cal_info(&delta))
//		print(LOG_DBG, "fail to read\n");

//	zc.hw_adc0 = 33;
//	zc.delta = 55;
//	ther_mtd_erase(get_mtd(), 0, m->erase_size, NULL);
//	ther_mtd_write(m, 0, &zc, sizeof(zc), NULL);
//	ther_mtd_read(m, 0, &zc2, sizeof(zc2), NULL);
//	print(LOG_DBG, "--> %d, %d\n", zc2.hw_adc0, zc2.delta);

//	print(LOG_DBG, "--> delta = %d\n", delta);
}
