
#ifndef __THER_UART_H__
#define __THER_UART_H__


enum {
	LOG_DBG = 1,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERR,
	LOG_CRIT
};

int print(unsigned char level, char *fmt, ...);

void ther_uart_init(unsigned char port, unsigned char baudrate,
		 void (*at_handle)(char *buf, unsigned char len, char *ret_buf, unsigned char *ret_len));
#endif

