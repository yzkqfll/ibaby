
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "string.h"

#include "ther_uart.h"
#include "thermometer.h"

#define MODULE "[THER AT] "

#define AT_CMD "AT"

void ther_at_handle(unsigned char *buf, unsigned short len, unsigned char *ret_buf, unsigned short *ret_len)
{

	if (memcmp(buf, AT_CMD, strlen(AT_CMD))) {
		/* not AT CMD */
		return;
	}

	if (buf[len - 1] == '\n' && buf[len - 2] == '\r') {
		len -= 2;
		buf[len] = '\0';
	} else if (buf[len - 1] == '\n') {
		len -= 1;
		buf[len] = '\0';
	} else {
		print(LOG_INFO, "unknown\r\n");
		return;
	}

	/* AT CMD */
	if (len == strlen(AT_CMD)) {
		return;
	}


}
