
#include "Comdef.h"
#include "OSAL.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal_board.h"

#include "ther_uart.h"
#include "thermometer.h"

#define MODULE "[THER AT] "

#define AT_CMD "AT"
#define AT_CAL "AT+CAL="
#define AT_CAL_QEREY "AT+CAL?"

static void at_set_cal_mode(bool mode)
{
	print(LOG_INFO, "set cal mode %d\n", mode);
}

void ther_at_handle(char *cmd_buf, unsigned short len, char *ret_buf, unsigned short *ret_len)
{
	char *p;

	*ret_len = 0;

	/* check cmd end */
	if (cmd_buf[len - 1] != '\n') {
		print(LOG_INFO, "cmd end with %c, should be \\n \n", cmd_buf[len - 1]);
		return;
	}

	/* remove \n */
	cmd_buf[len - 1] = '\0';
	len--;
/*	print(LOG_INFO, "get cmd <%s>, len %d\n", cmd_buf, len);*/

	if (strncmp(cmd_buf, AT_CMD, strlen(AT_CMD))) {
		print(LOG_INFO, "not AT cmd\n");
		return;
	}

	if (strcmp(cmd_buf, AT_CMD) == 0) {
		*ret_len = sprintf((char *)ret_buf, "%s\n", "OK");

	} else if (strncmp((char *)cmd_buf, AT_CAL, strlen(AT_CAL)) == 0) {
		p = cmd_buf + strlen(AT_CAL);
		if (*p == '1') {
			at_set_cal_mode(TRUE);
		} else if (*p == '0') {
			at_set_cal_mode(FALSE);
		} else {
/*			print(LOG_INFO, "buf <%s>, len %d, unknown cal mode\n",
					cmd_buf, len);*/
			*ret_len = sprintf((char *)ret_buf, "%s\n", "ERROR");
		}

	} else if (strncmp((char *)cmd_buf, AT_CAL_QEREY, strlen(AT_CAL_QEREY)) == 0) {

	} else {
/*		*ret_len = sprintf((char *)ret_buf, "UNKNOWN CMD\n");*/
		print(LOG_INFO, "UNKNOWN CMD\n");
	}

	return;

}
