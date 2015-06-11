
/*
 *
 */


#include "Comdef.h"
#include "OSAL.h"
#include "hal_uart.h"
#include "Hal_dma.h"

#include "ther_uart.h"

#define UART_NUM 1 /* Should HAL_UART_PORT_MAX */

#define UART_RX_BUF_LEN 128
#define UART_TX_BUF_LEN 128

struct uart_information {

	unsigned char rx_buf[UART_RX_BUF_LEN];

	void (*rx_handle)(unsigned char port, unsigned char *buf, unsigned char len);
};

/*
 * Actually we should dynamically allocate this,
 * But it will consume the RAM which is critical.
 * So we statically define the structure.
 */
static struct uart_information uart_info[UART_NUM];

static void uart_recv_isr(uint8 port, uint8 event)
{
	struct uart_information *ui = &uart_info[port];
	unsigned short len, len_read;
	unsigned char *buf;

	len_read = uart_recv(port, &buf, &len);

	if (len_read)
		ui->rx_handle(port, buf, len_read);

	return;
}

int uart_init(unsigned char port, unsigned char baud_rate,
			void (*hook)(unsigned char port, unsigned char *buf, unsigned char len))
{
	halUARTCfg_t config;
	struct uart_information *u;

	if (port >= UART_NUM)
		return -1;

	u = &uart_info[port];

	config.configured = TRUE;
	switch (baud_rate) {
	case UART_BAUD_RATE_9600:
		config.baudRate = HAL_UART_BR_9600;
		break;

	case UART_BAUD_RATE_19200:
		config.baudRate = HAL_UART_BR_19200;
		break;

	case UART_BAUD_RATE_38400:
		config.baudRate = HAL_UART_BR_38400;
		break;

	case UART_BAUD_RATE_115200:
		config.baudRate = HAL_UART_BR_115200;
		break;

	default:
		return -1;
	}

	config.flowControl = FALSE;
	if (config.flowControl)
		config.flowControlThreshold = 48;
	config.rx.maxBufSize = UART_RX_BUF_LEN;
	config.tx.maxBufSize = UART_TX_BUF_LEN;
	config.idleTimeout = 6;
	config.intEnable = TRUE;
	config.callBackFunc  = uart_recv_isr;

	u->rx_handle = hook;

	(void)HalUARTOpen(port, &config);

	return 0;
}

int uart_recv(int port, unsigned char **rx_buf, unsigned short *rx_len)
{
	struct uart_information *ui;
	unsigned short len, len_read;
	unsigned char *buf;

	ui = &uart_info[port];
	buf = ui->rx_buf;

	len = Hal_UART_RxBufLen(port);
	if (len == 0)
		return 0;

	if (len > UART_RX_BUF_LEN)
		len = UART_RX_BUF_LEN;

	len_read = HalUARTRead(port, buf, len);

	*rx_buf = buf;
	*rx_len = len_read;

	return len_read;
}

int uart_send(int port, unsigned char *buf, unsigned short len)
{

	return HalUARTWrite(port, buf, len);
}
