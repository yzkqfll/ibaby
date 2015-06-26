
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_mtd.h"

#include "ther_spi_w25x40cl.h"

#define MODULE "[THER MTD] "

struct mtd_info mtd;

void ther_mtd_init(void)
{
	struct mtd_info *m = &mtd;

	ther_spi_w25x_init(m);
}

