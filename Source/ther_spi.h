
#ifndef __THER_SPI_H__
#define __THER_SPI_H__

/**
 * SPI message structure
 */
struct ther_spi_message {
	const void *send_buf;
	void *recv_buf;
	uint32 length;
	unsigned cs_take    : 1;
	unsigned cs_release : 1;
};

/**
 * SPI common interface
 */
void ther_spi_init(void);
uint32 ther_spi_recv(void *recv_buf, uint32 length);
uint32 ther_spi_send(const void *send_buf, uint32 length);
uint32 ther_spi_send_then_send(const void *send_buf1, uint32 send_length1,
                               const void *send_buf2, uint32 send_length2);
uint32 ther_spi_send_then_recv(const void *send_buf, uint32 send_length,
                               void *recv_buf, uint32 recv_length);


#endif

