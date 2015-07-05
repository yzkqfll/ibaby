
#ifndef __THER_MISC_H__
#define __THER_MISC_H__


#define UART_WAIT 300

void start_watchdog_timer(void);
void feed_watchdog(void);
void delay(uint32 cnt);

#endif

