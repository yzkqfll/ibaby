
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_misc.h"

#define MODULE "[THER MISC] "

#define WDT_MODE_MASK 0x3
#define WDT_MODE_OFFSET 2
enum WDT_MODE {
	WDT_MODE_IDLE = 0,
	WDT_MODE_RESERVED,
	WDT_MODE_WATCHDOG,
	WDT_MODE_TIMER,
};

#define WDT_INT_MASK 0x3
#define WDT_INT_OFFSET 0
enum {
	WDT_INT_1S = 0,
	WDT_INT_250_MS,
	WDT_INT_15_MS,
	WDT_INT_1_MS
};

void start_watchdog_timer(void)
{
	WDCTL = (WDT_MODE_WATCHDOG << WDT_MODE_OFFSET) | (WDT_INT_1S << WDT_INT_OFFSET);
}

void feed_watchdog(void)
{
	WDCTL = 0xA0;
	WDCTL = 0x50;
}
