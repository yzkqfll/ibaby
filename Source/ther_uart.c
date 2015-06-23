
#include "Comdef.h"
#include "OSAL.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "ther_uart.h"
#include "ther_uart_drv.h"

#define MODULE "[UART COMM] "

static unsigned char log_level = LOG_DBG;

static unsigned char print_buf[UART_TX_BUF_LEN];

struct ther_uart {
	unsigned char port;
	char send_buf[UART_TX_BUF_LEN];

	unsigned char offset;
	char cmd_parse_buf[UART_TX_BUF_LEN + 1]; /* we will add \0 at the end */

	void (*data_handle)(char *buf, unsigned char len, char *ret_buf, unsigned char *ret_len);
};
struct ther_uart ther_uart[UART_PORT_NR];

#define PRINT_PORT UART_PORT_0

static void ther_uart_data_handle(unsigned char port, unsigned char *buf, unsigned char len)
{
	struct ther_uart *tu = &ther_uart[port];
	char *ret_buf = tu->send_buf;
	char *p, *end;
	unsigned char ret_len = UART_TX_BUF_LEN;

	if (tu->offset + len >= UART_TX_BUF_LEN) {
		tu->offset = 0;
		print(LOG_INFO, MODULE "overflow the cmd parse buffer\n");
		return;
	}

	memcpy(&tu->cmd_parse_buf[tu->offset], buf, len);
	tu->offset += len;
	tu->cmd_parse_buf[tu->offset] = '\0';
	p = strchr(tu->cmd_parse_buf, '\n');
	if (!p)
		return;

	if (tu->data_handle)
		tu->data_handle(tu->cmd_parse_buf, p - tu->cmd_parse_buf + 1, ret_buf, &ret_len);

	if (ret_len) {
		uart_drv_send(port, (unsigned char *)ret_buf, ret_len);
	}

	end = tu->cmd_parse_buf + tu->offset;
	if (p + 1 < end) {
		/* more data after *p */
		strncpy(tu->cmd_parse_buf, p + 1, end - p);
		tu->offset = end - p;
	} else {
		tu->offset = 0; /* err happened ?? */
	}

	/*
	 * add '\n' to send the data to uart immediately
	 */
//	uart_send(port, "\n", 2);

	return;
}

int print(unsigned char level, char *fmt, ...)
{
	va_list args;
	int n;

	if (level < log_level)
		return 0;

	va_start(args, fmt);
	n = vsprintf((char *)print_buf, fmt, args);
//	n = vsnprintf(print_buf, PRINT_BUF_LEN, fmt, args);
	if (n > 100)
		n = 100;
	uart_drv_send(UART_PORT_0, print_buf, n);
	va_end(args);

	return n;
}

/*
 * only support one UART now
 */
void ther_uart_init(unsigned char port, unsigned char baudrate,
		 void (*data_handle)(char *buf, unsigned char len, char *ret_buf, unsigned char *ret_len))
{
	struct ther_uart *tu = &ther_uart[port];

	memset(tu, 0, sizeof(struct ther_uart));

	tu->port = port;
	tu->data_handle = data_handle;

	uart_drv_init(port, baudrate, ther_uart_data_handle);

//	print(LOG_INFO, MODULE "uart init ok\n");

	return;
}
