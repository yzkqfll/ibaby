/*
 * THER AT COMMAND
 *
 * Copyright (c) 2015 by Leo Liu <59089403@qq.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 *
 * 2015/06/01 - Init version
 *              by Leo Liu <59089403@qq.com>
 *
 */

#include "bcomdef.h"
#include "Comdef.h"
#include "OSAL.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_temp.h"
#include "ther_adc.h"
#include "thermometer.h"
#include "ther_storage.h"
#include "ther_batt_service.h"
#include "ther_oled9639_drv.h"
#include "ther_port.h"
#include "ther_misc.h"

#define MODULE "[THER AT] "

#define AT_DELAY 10

#define AT_CMD "AT"
#define AT_MODE "AT+MODE="
#define AT_MODE_Q "AT+MODE"
#define AT_LDO "AT+LDO="
#define AT_LDO_Q "AT+LDO"
#define AT_ADC0 "AT+ADC0"
#define AT_ADC1 "AT+ADC1"
#define AT_HWADC0 "AT+HWADC0"
#define AT_HWADC1 "AT+HWADC1"
#define AT_HWADC6 "AT+HWADC6"
#define AT_CH0RT "AT+CH0RT"
#define AT_CH1RT "AT+CH1RT"
#define AT_TEMP0 "AT+CH0TEMP"
#define AT_TEMP1 "AT+CH1TEMP"

#define AT_ADC0_DELTA "AT+ADC0DELTA="
#define AT_ADC0_DELTA_Q "AT+ADC0DELTA"

#define AT_B_DELTA "AT+BDELTA="
#define AT_B_DELTA_Q "AT+BDELTA"
#define AT_R25_DELTA "AT+R25_DELTA="
#define AT_R25_DELTA_Q "AT+R25_DELTA"

#define AT_BATT_ADC "AT+BATTADC"
#define AT_BATT_VOLTAGE "AT+BATTV"
#define AT_BATT_PERCENTAGE "AT+BATTP"

#define AT_OLED_CONTRAST "AT+CONTRAST="

#define AT_ERASE_ALL_DATA "AT+ERASE"

#define AT_ALIVE "AT+ALIVE"
#define AT_TEST "AT+TEST"

static void enter_cal_mode(struct ther_info *ti)
{
	ti->mode = CAL_MODE;

	/*
	 * stop temp measurement
	 */
	osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);
	ther_temp_power_on();
	ti->temp_measure_stage = TEMP_STAGE_SETUP;

	/*
	 * stop batt measurement
	 */
	osal_stop_timerEx(ti->task_id, TH_BATT_EVT);
}


static unsigned char at_enter_cal_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_exit_cal_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	/*
	 * start temp measurement
	 */
	ther_temp_power_off();
	ti->temp_measure_stage = TEMP_STAGE_SETUP;
	osal_start_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT, AT_DELAY);

	/*
	 * start batt measurement
	 */
	osal_start_timerEx(ti->task_id, TH_BATT_EVT, AT_DELAY);

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

	return sprintf((char *)ret_buf, "+ADC%d:%d\n", channel, adc);
}

static unsigned char at_get_hw_adc(char *ret_buf, unsigned char channel)
{
	unsigned short adc = ther_get_hw_adc(channel);

	return sprintf((char *)ret_buf, "+HWADC%d:%d\n", channel, adc);
}

static unsigned char at_get_ch_Rt(char *ret_buf, unsigned char ch)
{
	float Rt;

	Rt = ther_get_Rt(ch);

	return sprintf((char *)ret_buf, "+CH%dRT:%f\n", ch, Rt);
}

static unsigned char at_get_ch_temp(char *ret_buf, unsigned char ch)
{
	unsigned short temp;

	temp = ther_get_ch_temp(ch);

	return sprintf((char *)ret_buf, "+CH%dTEMP:%d\n", ch, temp);
}

static unsigned char at_set_adc0_delta(char *ret_buf, short delta)
{
	ther_set_adc0_delta(delta);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_get_adc0_delta(char *ret_buf)
{
	unsigned short delta = ther_get_adc0_delta();

	return sprintf((char *)ret_buf, "+CH0 ADC DELTA:%d\n", delta);
}

static unsigned char at_set_B_delta(char *ret_buf, float delta)
{
	ther_set_B_delta(delta);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_get_B_delta(char *ret_buf)
{
	float delta = ther_get_B_delta();

	return sprintf((char *)ret_buf, "+BDELTA:%f\n", delta);
}

static unsigned char at_set_R25_delta(char *ret_buf, float delta)
{
	ther_set_R25_delta(delta);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned char at_get_R25_delta(char *ret_buf)
{
	float delta = ther_get_R25_delta();

	return sprintf((char *)ret_buf, "+R25DELTA:%f\n", delta);
}

static unsigned char at_get_batt_adc(char *ret_buf)
{
	unsigned short adc = ther_batt_get_adc();

	return sprintf((char *)ret_buf, "+BATTADC:%d\n", adc);
}

static unsigned char at_get_batt_voltage(char *ret_buf)
{
	float voltage = ther_batt_get_voltage();

	return sprintf((char *)ret_buf, "+BATTVOLTAGE:%.2f V\n", voltage);
}

static unsigned char at_get_batt_percentage(char *ret_buf)
{
	unsigned char percentage = ther_batt_get_percentage(TRUE);

	return sprintf((char *)ret_buf, "+BATT:%d%%\n", percentage);
}

static unsigned char at_set_oled9639_contrast(char *ret_buf, unsigned char contrast)
{
	oled_drv_set_contrast(contrast);

	return sprintf((char *)ret_buf, "OK\n");
}

static unsigned char at_erase_all_data(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	if (storage_erase())
		return sprintf((char *)ret_buf, "OK\n");
	else
		return sprintf((char *)ret_buf, "ERROR\n");
}

static unsigned char at_test(char *ret_buf)
{
	struct ther_info *ti = get_ti();
	bool ret;
	short delta;


//	ther_read_zero_cal_info(&delta);
//	print(LOG_INFO, MODULE "ADC0 delta %d\n", delta);

	ret = ther_storage_test();

	if (ret)
		return sprintf((char *)ret_buf, "OK\n");
	else
		return sprintf((char *)ret_buf, "ERROR\n");
//	while (1);
}

static void at_help(void)
{
	print(LOG_INFO, "--------------------------\n");

	print(LOG_INFO, "    AT\n");
	print(LOG_INFO, "    AT+MODE=\n");
	print(LOG_INFO, "    AT+MODE\n");
	print(LOG_INFO, "    AT+LDO=\n");
	print(LOG_INFO, "    AT+LDO\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+ADC0\n");
	print(LOG_INFO, "    AT+ADC1\n");
	print(LOG_INFO, "    AT+HWADC0\n");
	print(LOG_INFO, "    AT+HWADC1\n");
	print(LOG_INFO, "    AT+HWADC6\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CH0RT\n");
	print(LOG_INFO, "    AT+CH1RT\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CH0TEMP\n");
	print(LOG_INFO, "    AT+CH1TEMP\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+ADC0DELTA=\n");
	print(LOG_INFO, "    AT+ADC0DELTA\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+BDELTA=\n");
	print(LOG_INFO, "    AT+BDELTA\n");
	print(LOG_INFO, "    AT+R25_DELTA=\n");
	print(LOG_INFO, "    AT+R25_DELTA\n");
	print(LOG_INFO, "    AT+BDELTA=\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+BATTADC\n");
	print(LOG_INFO, "    AT+BATTV\n");
	print(LOG_INFO, "    AT+BATTP\n");

	delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CONTRAST=\n");

	delay(UART_WAIT);
	print(LOG_INFO, "--------------------------\n");
	print(LOG_INFO, "\n");
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
		at_help();
		return;
	}

	/* AT */
	if (strcmp(cmd_buf, AT_CMD) == 0) {
		*ret_len = sprintf((char *)ret_buf, "%s\n", "OK");

	} else if (strcmp(cmd_buf, AT_TEST) == 0) {
		*ret_len = at_test(ret_buf);

	} else if (strcmp(cmd_buf, AT_ALIVE) == 0) {


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

	/* AT+MODE */
	} else if (strcmp((char *)cmd_buf, AT_MODE_Q) == 0) {
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

	/* AT+ADC0 */
	} else if (strcmp((char *)cmd_buf, AT_ADC0) == 0) {
		*ret_len = at_get_adc(ret_buf, HAL_ADC_CHANNEL_0);

	/* AT+ADC1 */
	} else if (strcmp((char *)cmd_buf, AT_ADC1) == 0) {
		*ret_len = at_get_adc(ret_buf, HAL_ADC_CHANNEL_1);

	/* AT+HWADC0 */
	} else if (strcmp((char *)cmd_buf, AT_HWADC0) == 0) {
		*ret_len = at_get_hw_adc(ret_buf, HAL_ADC_CHANNEL_0);

	/* AT+HWADC1 */
	} else if (strcmp((char *)cmd_buf, AT_HWADC1) == 0) {
		*ret_len = at_get_hw_adc(ret_buf, HAL_ADC_CHANNEL_1);

	/* AT+HWADC6 */
	} else if (strcmp((char *)cmd_buf, AT_HWADC6) == 0) {
		*ret_len = at_get_hw_adc(ret_buf, HAL_ADC_CHANNEL_6);

	/* AT+CH0RT */
	} else if (strcmp((char *)cmd_buf, AT_CH0RT) == 0) {
		*ret_len = at_get_ch_Rt(ret_buf, HAL_ADC_CHANNEL_0);

	/* AT+CH1RT */
	} else if (strcmp((char *)cmd_buf, AT_CH1RT) == 0) {
		*ret_len = at_get_ch_Rt(ret_buf, HAL_ADC_CHANNEL_1);

	/* AT+TEMP0 */
	} else if (strcmp((char *)cmd_buf, AT_TEMP0) == 0) {
		*ret_len = at_get_ch_temp(ret_buf, HAL_ADC_CHANNEL_0);

	/* AT+TEMP1 */
	} else if (strcmp((char *)cmd_buf, AT_TEMP1) == 0) {
		*ret_len = at_get_ch_temp(ret_buf, HAL_ADC_CHANNEL_1);

	/* AT+ADC0DELTA=x */
	} else if (strncmp((char *)cmd_buf, AT_ADC0_DELTA, strlen(AT_ADC0_DELTA)) == 0) {
		short delta;

		p = cmd_buf + strlen(AT_ADC0_DELTA);
		delta =  atoi(p);

		*ret_len = at_set_adc0_delta(ret_buf, delta);

	/* AT+ADC0DELTA */
	} else if (strcmp((char *)cmd_buf, AT_ADC0_DELTA_Q) == 0) {
		*ret_len = at_get_adc0_delta(ret_buf);

	/* AT+BDELTA=x */
	} else if (strncmp((char *)cmd_buf, AT_B_DELTA, strlen(AT_B_DELTA)) == 0) {
		float delta;

		p = cmd_buf + strlen(AT_B_DELTA);
		delta =  atof(p);

		*ret_len = at_set_B_delta(ret_buf, delta);

	/* AT+BDELTA */
	} else if (strcmp((char *)cmd_buf, AT_B_DELTA_Q) == 0) {
		*ret_len = at_get_B_delta(ret_buf);

	/* AT+R25_DELTA=x */
	} else if (strncmp((char *)cmd_buf, AT_R25_DELTA, strlen(AT_R25_DELTA)) == 0) {
		float delta;

		p = cmd_buf + strlen(AT_R25_DELTA);
		delta =  atof(p);

		*ret_len = at_set_R25_delta(ret_buf, delta);

	/* AT+R25_DELTA= */
	} else if (strcmp((char *)cmd_buf, AT_R25_DELTA_Q) == 0) {
		*ret_len = at_get_R25_delta(ret_buf);

	/* AT+BATTADC */
	} else if (strcmp((char *)cmd_buf, AT_BATT_ADC) == 0) {
		*ret_len = at_get_batt_adc(ret_buf);

	/* AT+BATTV */
	} else if (strcmp((char *)cmd_buf, AT_BATT_VOLTAGE) == 0) {
		*ret_len = at_get_batt_voltage(ret_buf);

	/* AT+BATTP */
	} else if (strcmp((char *)cmd_buf, AT_BATT_PERCENTAGE) == 0) {
		*ret_len = at_get_batt_percentage(ret_buf);

	/* AT+CONTRAST=x */
	} else if (strncmp((char *)cmd_buf, AT_OLED_CONTRAST, strlen(AT_OLED_CONTRAST)) == 0) {
		unsigned char contrast;

		p = cmd_buf + strlen(AT_OLED_CONTRAST);
		contrast =  atoi(p);

		*ret_len = at_set_oled9639_contrast(ret_buf, contrast);

	/* AT+ERASE */
	} else if (strcmp((char *)cmd_buf, AT_ERASE_ALL_DATA) == 0) {
		*ret_len = at_erase_all_data(ret_buf);

	} else {
		at_help();
	}

	return;

}
