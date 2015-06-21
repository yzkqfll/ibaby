
#ifndef __THER_UART_DRV_H__
#define __THER_UART_DRV_H__

int uart_init(unsigned char port, unsigned char baud_rate,
			void (*hook)(unsigned char port, unsigned char *buf, unsigned short len));

int uart_drv_recv(int port, unsigned char *buf, unsigned short *len);
int uart_drv_send(int port, unsigned char *buf, unsigned short len);

enum {
	UART_PORT_0 = 0,
	UART_PORT_1,
	UART_PORT_NR = 1 /* save rom space */
};

/*
 * Baudrate
 */
enum {
	UART_BAUD_RATE_9600,
	UART_BAUD_RATE_19200,
	UART_BAUD_RATE_38400,
	UART_BAUD_RATE_115200,
};

#define UART_RX_BUF_LEN 128
#define UART_TX_BUF_LEN 128

#endif

