
/*
 *
 */


#include "Comdef.h"
#include "OSAL.h"
#include "hal_uart.h"
#include "Hal_dma.h"

#include "ther_uart_drv.h"

struct ther_uart_drv {
	unsigned char rx_buf[UART_RX_BUF_LEN];
	void (*rx_handle)(unsigned char port, unsigned char *buf, unsigned short len);
};

/*
 * Actually we should dynamically allocate this,
 * But it will consume the RAM which is critical.
 * So we statically define the structure.
 */
static struct ther_uart_drv uart_drv[UART_PORT_NR];

int uart_drv_recv(int port, unsigned char *buf, unsigned short *len)
{
	unsigned short rx_buf_len;

	rx_buf_len = Hal_UART_RxBufLen(port);
	if (rx_buf_len == 0)
		return 0;

	if (rx_buf_len > *len)
		rx_buf_len = *len;

	*len = HalUARTRead(port, buf, rx_buf_len);

	return *len;
}

static void uart_recv_isr(uint8 port, uint8 event)
{
	struct ther_uart_drv *ud = &uart_drv[port];
	unsigned short len = UART_RX_BUF_LEN, len_read;
	unsigned char *buf = ud->rx_buf;

	len_read = uart_drv_recv(port, buf, &len);

	if (len_read)
		ud->rx_handle(port, buf, len_read);

	return;
}

int uart_drv_send(int port, unsigned char *buf, unsigned short len)
{

	return HalUARTWrite(port, buf, len);
}

int uart_init(unsigned char port, unsigned char baud_rate,
			void (*hook)(unsigned char port, unsigned char *buf, unsigned short len))
{
	halUARTCfg_t config;
	struct ther_uart_drv *ud;

	if (port >= UART_PORT_NR)
		return -1;

/*
#if DMA_PM
	P0SEL &= ~0x30;
	P0DIR |= 0x30;
	P0 &= ~0x30;
#endif
*/

	ud = &uart_drv[port];

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

	ud->rx_handle = hook;

	(void)HalUARTOpen(port, &config);

	return 0;
}


