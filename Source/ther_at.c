
#include "Comdef.h"
#include "OSAL.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_temp.h"
#include "thermometer.h"

#define MODULE "[THER AT] "

#define AT_DELAY 10

#define AT_CMD "AT"
#define AT_MODE "AT+MODE="
#define AT_MODE_QUREY "AT+MODE?"
#define AT_LDO "AT+LDO="
#define AT_LDO_QUREY "AT+LDO?"
#define AT_ADC0 "AT+ADC0?"
#define AT_ADC1 "AT+ADC1?"
#define AT_TEMP0 "AT+TEMP0?"
#define AT_TEMP1 "AT+TEMP1?"

static unsigned char at_enter_cal_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode == CAL_MODE) {
		return sprintf((char *)ret_buf, "%s\n", "Already in cal mode");
	}

	/* test */
	osal_stop_timerEx(ti->task_id, TH_TEST_EVT);

	/*
	 * stop temp measurement
	 */
	osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);
	ther_temp_power_off();
	ti->temp_measure_stage = TEMP_STAGE_SETUP;

//	print(LOG_DBG, MODULE "enter cal mode\n");
	ti->mode = CAL_MODE;

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_exit_cal_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode == NORMAL_MODE) {
		return sprintf((char *)ret_buf, "%s\n", "Already in normal mode");
	}

	/* test */
	osal_start_timerEx(ti->task_id, TH_TEST_EVT, AT_DELAY);

	/*
	 * stop temp measurement
	 */
	ther_temp_power_off();
	ti->temp_measure_stage = TEMP_STAGE_SETUP;
	osal_start_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT, AT_DELAY);

//	print(LOG_DBG, MODULE "exit cal mode\n");
	ti->mode = NORMAL_MODE;

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_get_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();
	unsigned char cnt;

	if (ti->mode == NORMAL_MODE) {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:NORMAL");
	} else if (ti->mode == CAL_MODE) {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:CAL");
	} else {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:UNKNOWN");
	}

	return cnt;
}

static unsigned char at_set_ldo_on(char *ret_buf)
{
	ther_temp_power_on();

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_set_ldo_off(char *ret_buf)
{
	ther_temp_power_off();

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_get_adc(char *ret_buf, unsigned char channel)
{
	unsigned short adc = ther_get_adc(channel);

//	print(LOG_DBG, "adc %d: %d\n", channel, adc);

	return sprintf((char *)ret_buf, "+ADC%d:%d\n", channel, adc);
}

void ther_at_handle(char *cmd_buf, unsigned char len, char *ret_buf, unsigned char *ret_len)
{
	char *p;

	*ret_len = 0;

	/* check cmd end */
	if (cmd_buf[len - 1] != '\n') {
		print(LOG_DBG, "cmd end with %c, should be \\n \n", cmd_buf[len - 1]);
		return;
	}

	/* remove \n */
	cmd_buf[len - 1] = '\0';
	len--;
//	print(LOG_DBG, "get cmd <%s>, len %d\n", cmd_buf, len);

	if (strncmp(cmd_buf, AT_CMD, strlen(AT_CMD))) {
		*ret_len = sprintf((char *)ret_buf, "%s\n", "Not AT cmd");
		return;
	}

	/* AT */
	if (strcmp(cmd_buf, AT_CMD) == 0) {
		*ret_len = sprintf((char *)ret_buf, "%s\n", "OK");

	/* AT+MODE=x */
	} else if (strncmp((char *)cmd_buf, AT_MODE, strlen(AT_MODE)) == 0) {
		p = cmd_buf + strlen(AT_MODE);
		if (*p == '1') {
			*ret_len = at_enter_cal_mode(ret_buf);
		} else if (*p == '0') {
			*ret_len = at_exit_cal_mode(ret_buf);
		} else {
			*ret_len = sprintf((char *)ret_buf, "%s\n", "+MODE:UNKNOWN");
		}

	/* AT+MODE? */
	} else if (strncmp((char *)cmd_buf, AT_MODE_QUREY, strlen(AT_MODE_QUREY)) == 0) {
		*ret_len = at_get_mode(ret_buf);

	/* AT+LDO=x */
	} else if (strncmp((char *)cmd_buf, AT_LDO, strlen(AT_LDO)) == 0) {
		p = cmd_buf + strlen(AT_LDO);
		if (*p == '1') {
			*ret_len = at_set_ldo_on(ret_buf);
		} else if (*p == '0') {
			*ret_len = at_set_ldo_off(ret_buf);
		} else {
			*ret_len = sprintf((char *)ret_buf, "%s\n", "+LDO:UNKNOWN");
		}

	/* AT+ADC0? */
	} else if (strncmp((char *)cmd_buf, AT_ADC0, strlen(AT_ADC0)) == 0) {
		*ret_len = at_get_adc(ret_buf, 0);

	/* AT+ADC1? */
	} else if (strncmp((char *)cmd_buf, AT_ADC1, strlen(AT_ADC1)) == 0) {
		*ret_len = at_get_adc(ret_buf, 1);

	/* AT+TEMP0? */
	} else if (strncmp((char *)cmd_buf, AT_TEMP0, strlen(AT_TEMP0)) == 0) {

	/* AT+TEMP1? */
	} else if (strncmp((char *)cmd_buf, AT_TEMP1, strlen(AT_TEMP1)) == 0) {

	} else {
		*ret_len = sprintf((char *)ret_buf, "%s\n", "Unknown AT cmd");
	}

	return;

}
