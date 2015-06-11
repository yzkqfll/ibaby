
#ifndef __THER_UART_H__
#define __THER_UART_H__

int uart_init(unsigned char port, unsigned char baud_rate,
			void (*hook)(unsigned char port, unsigned char *buf, unsigned char len));

int uart_recv(int port, unsigned char **rx_buf, unsigned short *rx_len);
int uart_send(int port, unsigned char *buf, unsigned short len);

enum {
	UART_BAUD_RATE_9600,
	UART_BAUD_RATE_19200,
	UART_BAUD_RATE_38400,
	UART_BAUD_RATE_115200,
};

#endif

