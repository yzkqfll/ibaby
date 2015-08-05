
#ifndef __THER_MISC_H__
#define __THER_MISC_H__


#define UART_WAIT 400

void start_watchdog_timer(uint8 interval);
void feed_watchdog(void);
void delay(uint32 cnt);
void uart_delay(uint32 cnt);
const char *get_reset_reason(void);
void system_reset(void);

#endif

