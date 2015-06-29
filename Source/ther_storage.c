
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
    int a;
};

void ther_storage_test(void)
{
	struct mtd_info*m = get_mtd();
	bool ret;

	m->open();

//	ret = m->erase(0, m->erase_size);
//	if (!ret) {
//		print(LOG_DBG, MODULE "fail to erase at offset 0x%x, len 0x%x\n", 0, m->erase_size);
//		return;
//	}



	m->close();
}
