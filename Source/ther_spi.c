/*
 * TI CC254X SPI driver using I/O pin
 *
 * Change Log:
 * =======================
 * 6Jun15, yue.hu, created.
 */

#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_spi.h"

#define MODULE "[SPI] "

/**
 * SPI I/O pin definitions
 */
#define SPI_PIN_CS     P1_4
#define SPI_PIN_SCK    P1_5
#define SPI_PIN_MOSI   P1_6
#define SPI_PIN_MISO   P1_7
#define SPI_PIN_WP     P2_4
#define SPI_PIN_VCC    P1_1

/**
 * SPI I/O operations
 */
#define THER_SPI_RX()         U1DBUF
#define THER_SPI_TX(x)        st(U1DBUF = (x);)
#define THER_SPI_WAIT_RXRDY() st(while (!(U1CSR & 0x02));)
#define THER_SPI_CLR_RXRDY()  st(U1CSR &= ~0x02;)
#define THER_SPI_EN(x)        st(SPI_PIN_CS = (x);)
#define THER_SPI_WP(x)        st(SPI_PIN_WP = (x);)
#define THER_SPI_POWER(x)     st(SPI_PIN_VCC = (x);)


void ther_spi_init(void)
{
	/* Set UART1 I/O to Alt. 2 location on P1 */
	PERCFG |= 0x02;

	/* Mode is SPI-Master Mode */
	U1CSR = 0;

	/* FixME: Cfg for the max Rx/Tx baud of 2-MHz */
	U1GCR =  15;
	U1BAUD = 255;

	/* Flush it */
	U1UCR = 0x80;

	/* Set bit order to MSB */
	U1GCR |= BV(5);

	/* M1/MO/C config */
	P1SEL |= 0xE0;
	P1DIR |= BV(6);
	P1DIR &= ~ BV(7);

	/* Disable interrupt */
	P1IEN &= ~BV(4);
	P1IEN &= ~BV(5);
	P1IEN &= ~BV(6);
	P1IEN &= ~BV(7);

	/* CS config */
	P1SEL &= ~ BV(4);
	P1DIR |= BV(4);
	P1INP |= BV(4);

	/* VCC config */
	P1SEL &= ~ BV(1);
	P1DIR |= BV(1);
	P1INP |= BV(1);

	/* WP config */
	P2SEL &= ~ BV(2);
	P2DIR |= BV(4);
	P2INP |= BV(4);

	/* Receiver enable */
	U1CSR |= 0x40;

	THER_SPI_POWER(1);
	THER_SPI_WP(0);
	THER_SPI_EN(0);
}

static uint32 ther_spi_xfer(struct ther_spi_message* message)
{
	uint32 size = message->length;
	const uint8 * send_ptr = message->send_buf;
	uint8 * recv_ptr = message->recv_buf;
	uint8 data = 0xFF;

	if(message->cs_take) {
		THER_SPI_EN(0);
	}

	while(size--) {
		if(send_ptr != NULL) {
			data = *send_ptr++;
		}

		/* Send the byte */
		THER_SPI_TX(data);

		/* Wait until the transfer end */
		THER_SPI_WAIT_RXRDY();

		/* Get the received data */
		data = THER_SPI_RX();

		/* Clear the tranfer end status */
		THER_SPI_CLR_RXRDY();

		if(recv_ptr != NULL) {
			*recv_ptr++ = data;
		}
	}

	if(message->cs_release) {
		THER_SPI_EN(1);
	}

	return message->length;
}

uint32 ther_spi_recv(void *recv_buf, uint32 length)
{
	struct ther_spi_message message;

	message.send_buf   = NULL;
	message.recv_buf   = recv_buf;
	message.length     = length;
	message.cs_take    = 1;
	message.cs_release = 1;

	return ther_spi_xfer(&message);
}

uint32 ther_spi_send(const void *send_buf, uint32 length)
{
	struct ther_spi_message message;

	message.send_buf   = send_buf;
	message.recv_buf   = NULL;
	message.length     = length;
	message.cs_take    = 1;
	message.cs_release = 1;

	return ther_spi_xfer(&message);
}

uint32 ther_spi_send_then_send(const void *send_buf1, uint32 send_length1,
                               const void *send_buf2, uint32 send_length2)
{
	struct ther_spi_message message;

	/* send data1 */
	message.send_buf   = send_buf1;
	message.recv_buf   = NULL;
	message.length     = send_length1;
	message.cs_take    = 1;
	message.cs_release = 0;
	ther_spi_xfer(&message);

	/* send data2 */
	message.send_buf   = send_buf2;
	message.recv_buf   = NULL;
	message.length     = send_length2;
	message.cs_take    = 0;
	message.cs_release = 1;
	ther_spi_xfer(&message);

	return 0;
}

uint32 ther_spi_send_then_recv(const void *send_buf, uint32 send_length,
                               void *recv_buf, uint32 recv_length)
{
	struct ther_spi_message message;

	/* send data1 */
	message.send_buf   = send_buf;
	message.recv_buf   = NULL;
	message.length     = send_length;
	message.cs_take    = 1;
	message.cs_release = 0;
	ther_spi_xfer(&message);

	/* send data2 */
	message.send_buf   = NULL;
	message.recv_buf   = recv_buf;
	message.length     = recv_length;
	message.cs_take    = 0;
	message.cs_release = 1;
	ther_spi_xfer(&message);

	return 0;
}




