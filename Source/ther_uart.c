
#include "Comdef.h"
#include "OSAL.h"
#include <stdarg.h>
#include <stdio.h>

#include "ther_uart.h"
#include "ther_uart_drv.h"

#define MODULE "[UART COMM] "

static unsigned char log_level = LOG_DBG;

static unsigned char send_buf[UART_TX_BUF_LEN];

struct ther_uart {
	unsigned char port;

	void (*data_handle)(unsigned char *buf, unsigned short len, unsigned char *ret_buf, unsigned short *ret_len);
};
struct ther_uart ther_uart[UART_PORT_NR];

#define PRINT_PORT UART_PORT_0

static void ther_uart_data_handle(unsigned char port, unsigned char *buf, unsigned short len)
{
	struct ther_uart *tu = &ther_uart[port];
	unsigned char *ret_buf = send_buf;
	unsigned short ret_len = 0;

	if (tu->data_handle)
		tu->data_handle(buf, len, ret_buf, &ret_len);

	if (ret_len)
		uart_drv_send(port, ret_buf, ret_len);

	/*
	 * add '\n' to send the data to uart immediately
	 */
//	uart_send(port, "\n", 2);

	return;
}

int print(unsigned char level, char *fmt, ...)
{
	struct ther_uart *tu = &ther_uart[PRINT_PORT];
	va_list args;
	int n;

	if (level < log_level)
		return 0;

	va_start(args, fmt);
	n = vsprintf((unsigned char *)send_buf, fmt, args);
//	n = vsnprintf(print_buf, PRINT_BUF_LEN, fmt, args);
	if (n > 100)
		n = 100;
	uart_drv_send(tu->port, (unsigned char *)send_buf, n);
	va_end(args);

	return n;
}

/*
 * only support one UART now
 */
void ther_uart_init(unsigned char port, unsigned char baudrate,
		 void (*data_handle)(unsigned char *buf, unsigned short len, unsigned char *ret_buf, unsigned short *ret_len))
{
	struct ther_uart *tu = &ther_uart[port];

	tu->port = port;
	tu->data_handle = data_handle;

	uart_init(port, baudrate, ther_uart_data_handle);

//	print(LOG_INFO, MODULE "uart init ok\n");

	return;
}
