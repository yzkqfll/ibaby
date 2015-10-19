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
#include "ther_data.h"
#include "ther_ble.h"

#define MODULE "[THER AT] "

#define AT_DELAY 10

/* system */
#define AT_CMD "AT"
#define AT_MODE "AT+MODE="
#define AT_MODE_Q "AT+MODE"
#define AT_RESET "AT+RESET"

#define AT_MAC "AT+MAC"

#define AT_HIGH_TEMP_WARNING "AT+HTW="
#define AT_HIGH_TEMP_WARNING_Q "AT+HTW"

/* zero cal */
#define AT_LDO "AT+LDO="
#define AT_LDO_Q "AT+LDO"
#define AT_ADC0 "AT+ADC0"
#define AT_ADC1 "AT+ADC1"
#define AT_HWADC0 "AT+HWADC0"
#define AT_HWADC1 "AT+HWADC1"
#define AT_ADC0_DELTA "AT+ADC0DELTA="
#define AT_ADC0_DELTA_Q "AT+ADC0DELTA"

#define AT_CH0RT "AT+CH0RT"
#define AT_CH1RT "AT+CH1RT"
#define AT_TEMP0 "AT+CH0TEMP"
#define AT_TEMP1 "AT+CH1TEMP"

/* temp cal */
#define AT_LOW_TEMP_CAL "AT+LOWTEMPCAL="
#define AT_LOW_TEMP_CAL_Q "AT+LOWTEMPCAL"
#define AT_HIGH_TEMP_CAL "AT+HIGHTEMPCAL="
#define AT_HIGH_TEMP_CAL_Q "AT+HIGHTEMPCAL"

#define AT_TEMP_CAL "AT+TEMPCAL="
#define AT_TEMP_CAL_Q "AT+TEMPCAL"

/* batt */
#define AT_BATT_ADC "AT+BATTADC"
#define AT_BATT_VOLTAGE "AT+BATTV"
#define AT_BATT_PERCENTAGE "AT+BATTP"

/* oled */
#define AT_OLED_CONTRAST "AT+CONTRAST="

/* spi/storage */
#define AT_S_ERASE "AT+SERASE"
#define AT_S_INFO "AT+SINFO"
#define AT_S_RESET "AT+SRESET"
#define AT_S_RESTORE "AT+SRESTORE"
#define AT_S_TEST "AT+STEST"

/* misc */
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


	/*
	 * stop auto power off
	 */
	osal_stop_timerEx(ti->task_id, TH_AUTO_POWER_OFF_EVT);

	osal_stop_timerEx(ti->task_id, TH_HIGH_TEMP_WARNING_EVT);
	osal_stop_timerEx(ti->task_id, TH_LOW_BATT_WARNING_EVT);
}


static uint8 at_enter_cal_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_exit_cal_mode(char *ret_buf)
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

static uint8 at_get_mode(char *ret_buf)
{
	struct ther_info *ti = get_ti();
	uint8 cnt;

	if (ti->mode == NORMAL_MODE) {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:NORMAL");
	} else if (ti->mode == CAL_MODE) {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:CAL");
	} else {
		cnt = sprintf((char *)ret_buf, "%s\n", "+MODE:UNKNOWN");
	}

	return cnt;
}

static uint8 at_reset(char *ret_buf)
{
	system_reset();

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_get_mac(char *ret_buf)
{
	uint8 mac[B_ADDR_LEN] = {0};
	ble_get_mac(mac);

	return sprintf((char *)ret_buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

static uint8 at_set_htw(char *ret_buf, uint8 warning_enabled, uint16 high_temp_threshold)
{
	struct ther_info *ti = get_ti();

	/* read high temp thershold */
	if (!storage_write_high_temp_enabled(warning_enabled)) {
		return sprintf((char *)ret_buf, "%s\n", "ERROR");
	}

	if (!storage_write_high_temp_threshold(high_temp_threshold)) {
		return sprintf((char *)ret_buf, "%s\n", "ERROR");
	}

	ti->warning_enabled = warning_enabled;
	ti->high_temp_threshold = high_temp_threshold;
	ti->next_warning_threshold = ti->high_temp_threshold;

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_get_htw(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	return sprintf((char *)ret_buf, "%d,%d,%d\n",
			ti->warning_enabled, ti->high_temp_threshold, ti->next_warning_threshold);
}


static uint8 at_set_ldo_on(char *ret_buf)
{
	ther_temp_power_on();

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_set_ldo_off(char *ret_buf)
{
	ther_temp_power_off();

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static unsigned short get_ave_val(unsigned short val[], uint16 num)
{
	uint16 i, j, max_index;
	uint16 tmp;
	uint32 sum = 0;

/*	for (i = 0; i < num; i++) {
		print(LOG_DBG, MODULE "%d: %d\n", i, val[i]);
	}*/

	if (num < 3)
		return val[num / 2];

	for (i = 0; i < num; i++) {
		max_index = i;
		for (j = i + 1; j < num; j++) {
			if (val[max_index] < val[j]) {
				max_index = j;
			}
		}
		tmp = val[i];
		val[i] = val[max_index];
		val[max_index] = tmp;
	}

/*	for (i = 0; i < num; i++) {
		print(LOG_DBG, MODULE "%d: %d\n", i, val[i]);
	}*/

#if 0
	for (i = num / 2 - 5; i < num / 2 + 5; i++) {
		sum += val[i];
	}
//	print(LOG_DBG, MODULE "sum %ld\n", sum);
	return sum / 10;
#endif
	for (i = 0; i < num; i++) {
		sum += val[i];
	}
//	print(LOG_DBG, MODULE "sum %ld, avg is %ld\n", sum, sum / num);
	return sum / num;

}

static uint8 at_get_adc(char *ret_buf, uint8 channel)
{
	unsigned short adc = ther_get_adc(channel);

	return sprintf((char *)ret_buf, "+ADC%d:%d\n", channel, adc);
}

#define CH0_SAMPLE_NUM 150
static uint8 at_get_hw_adc(char *ret_buf, uint8 channel)
{
	unsigned short adc = ther_get_hw_adc(channel);

	if (channel == HAL_ADC_CHANNEL_0) {
		uint16 i;
		uint16 sample_adc[CH0_SAMPLE_NUM] = {0};

		for (i = 0; i < CH0_SAMPLE_NUM; i++) {
			sample_adc[i] = ther_get_hw_adc(channel);
		}
		adc = get_ave_val(sample_adc, CH0_SAMPLE_NUM);
	}

	return sprintf((char *)ret_buf, "+HWADC%d:%d\n", channel, adc);
}

static uint8 at_get_ch_Rt(char *ret_buf, uint8 ch)
{
	float Rt;

	Rt = ther_get_Rt(ch);

	return sprintf((char *)ret_buf, "+CH%dRT:%.4f\n", ch, Rt);
}

static uint8 at_get_ch_temp(char *ret_buf, uint8 ch)
{
	float temp;

	temp = ther_get_ch_temp(ch);

	return sprintf((char *)ret_buf, "+CH%dTEMP:%.4f\n", ch, temp);
}

static uint8 at_set_adc0_delta(char *ret_buf, short delta)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	ther_set_adc0_delta(delta);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_get_adc0_delta(char *ret_buf)
{
	short delta = ther_get_adc0_delta();

	return sprintf((char *)ret_buf, "+CH0 ADC DELTA:%d\n", delta);
}

static uint8 at_set_low_temp_cal(char *ret_buf, float R_low, float t_low)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	if (storage_write_low_temp_cal(R_low, t_low))
		return sprintf((char *)ret_buf, "%s\n", "OK");
	else
		return sprintf((char *)ret_buf, "%s\n", "ERROR");
}

static uint8 at_get_low_temp_cal(char *ret_buf)
{
	float R_low, t_low;

	storage_read_low_temp_cal(&R_low, &t_low);

	return sprintf((char *)ret_buf, "+LOWTEMPCAL:%.4f,%.4f\n", R_low, t_low);
}

static uint8 at_set_high_temp_cal(char *ret_buf, float R_high, float t_high)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	if (storage_write_high_temp_cal(R_high, t_high))
		return sprintf((char *)ret_buf, "%s\n", "OK");
	else
		return sprintf((char *)ret_buf, "%s\n", "ERROR");
}

static uint8 at_get_high_temp_cal(char *ret_buf)
{
	float R_high, t_high;

	storage_read_high_temp_cal(&R_high, &t_high);

	return sprintf((char *)ret_buf, "+HIGHTEMPCAL:%.4f,%.4f\n", R_high, t_high);
}

static uint8 at_set_temp_cal(char *ret_buf, float B_delta, float R25_delta)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	ther_set_temp_delta(B_delta, R25_delta);

	return sprintf((char *)ret_buf, "%s\n", "OK");
}

static uint8 at_get_temp_cal(char *ret_buf)
{
	float B_delta, R25_delta;

	ther_get_temp_cal(&B_delta, &R25_delta);

	return sprintf((char *)ret_buf, "+TEMPCAL:%.4f,%.4f\n", B_delta, R25_delta);
}

static uint8 at_get_batt_adc(char *ret_buf)
{
	unsigned short adc = ther_batt_get_adc();

	return sprintf((char *)ret_buf, "+BATTADC:%d\n", adc);
}

static uint8 at_get_batt_voltage(char *ret_buf)
{
	float voltage = ther_batt_get_voltage();

	return sprintf((char *)ret_buf, "+BATTVOLTAGE:%.2f V\n", voltage);
}

static uint8 at_get_batt_percentage(char *ret_buf)
{
	uint8 percentage = ther_batt_get_percentage(TRUE);

	return sprintf((char *)ret_buf, "+BATT:%d%%\n", percentage);
}

static uint8 at_set_oled9639_contrast(char *ret_buf, uint8 contrast)
{
	oled_drv_set_contrast(contrast);

	return sprintf((char *)ret_buf, "OK\n");
}

static uint8 at_serase(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	if (storage_erase())
		return sprintf((char *)ret_buf, "OK\n");
	else
		return sprintf((char *)ret_buf, "ERROR\n");
}

static uint8 at_sinfo(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	storage_show_info();

	return 0;
}

static uint8 at_sreset(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	storage_reset();

	return 0;
}

static uint8 at_srestore(char *ret_buf)
{
	uint8 *data;
	uint16 len;
	uint16 offset = 0;
	struct temp_data *td;
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	storage_restore_temp(&data, &len);

	if (data) {
		print(LOG_DBG, "get data len %d\n", len);
		for (offset = 0; offset < len; offset += sizeof(struct temp_data)) {
			td = (struct temp_data *)(data + offset);
			print(LOG_DBG, "%d-%02d-%02d %02d:%02d:%02d, temp %lx\n",
					td->year, td->month, td->day, td->hour, td->minutes, td->seconds,
					td->temp);
		}
	}

	return 0;
}

static uint8 at_stest(char *ret_buf)
{
	struct ther_info *ti = get_ti();

	if (ti->mode != CAL_MODE)
		enter_cal_mode(ti);

	storage_test();

	return 0;
}

static uint8 at_test(char *ret_buf)
{
	struct ther_info *ti = get_ti();
	bool ret = TRUE;

	{
		int16 temp = -38.46 * 100;
		uint8 first_num, sec_num, third_num, forth_num;

		first_num = temp / 1000;
		sec_num = (temp / 100) % 10;
		third_num = (temp / 10) % 10;
		forth_num = temp % 10;

		print(LOG_DBG, "%d, %d, %d, %d\n", first_num, sec_num, third_num, forth_num);

		temp = -temp;
		first_num = temp / 1000;
		sec_num = (temp / 100) % 10;
		third_num = (temp / 10) % 10;
		forth_num = temp % 10;

		print(LOG_DBG, "%d, %d, %d, %d\n", first_num, sec_num, third_num, forth_num);
	}

	if (ret)
		return sprintf((char *)ret_buf, "OK\n");
	else
		return sprintf((char *)ret_buf, "ERROR\n");
}

static void at_help(void)
{
	print(LOG_INFO, "--------------------------\n");

	print(LOG_INFO, "    AT\n");
	print(LOG_INFO, "    AT+MODE=\n");
	print(LOG_INFO, "    AT+MODE\n");
	print(LOG_INFO, "    AT+RESET\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+LDO=\n");
	print(LOG_INFO, "    AT+LDO\n");
	print(LOG_INFO, "    AT+ADC0\n");
	print(LOG_INFO, "    AT+ADC1\n");
	print(LOG_INFO, "    AT+HWADC0\n");
	print(LOG_INFO, "    AT+HWADC1\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+ADC0DELTA=\n");
	print(LOG_INFO, "    AT+ADC0DELTA\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CH0RT\n");
	print(LOG_INFO, "    AT+CH1RT\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CH0TEMP\n");
	print(LOG_INFO, "    AT+CH1TEMP\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+LOWTEMPCAL=\n");
	print(LOG_INFO, "    AT+LOWTEMPCAL\n");
	print(LOG_INFO, "    AT+HIGHTEMPCAL=\n");
	print(LOG_INFO, "    AT+HIGHTEMPCAL\n");
	uart_delay(UART_WAIT);
	print(LOG_INFO, "    AT+TEMPCAL=\n");
	print(LOG_INFO, "    AT+TEMPCAL\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+BATTADC\n");
	print(LOG_INFO, "    AT+BATTV\n");
	print(LOG_INFO, "    AT+BATTP\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+CONTRAST=\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "\n");
	print(LOG_INFO, "    AT+SERASE\n");
	print(LOG_INFO, "    AT+SINFO\n");
	print(LOG_INFO, "    AT+SRESET\n");
	print(LOG_INFO, "    AT+SRESTORE\n");
	print(LOG_INFO, "    AT+STEST\n");

	uart_delay(UART_WAIT);
	print(LOG_INFO, "--------------------------\n");
	print(LOG_INFO, "\n");
}

void ther_at_handle(char *cmd_buf, uint8 len, char *ret_buf, uint8 *ret_len)
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

	/* AT+RESET */
	} else if (strcmp((char *)cmd_buf, AT_RESET) == 0) {
		*ret_len = at_reset(ret_buf);

	/* AT+MAC */
	} else if (strcmp((char *)cmd_buf, AT_MAC) == 0) {
		*ret_len = at_get_mac(ret_buf);

	/* AT+HTW=x,x */
	} else if (strncmp((char *)cmd_buf, AT_HIGH_TEMP_WARNING, strlen(AT_HIGH_TEMP_WARNING)) == 0) {
		uint8 warning_enabled;
		uint16 high_temp_threshold;

		p = cmd_buf + strlen(AT_HIGH_TEMP_WARNING);
		warning_enabled =  atoi(p);

		p = strstr((const char *)cmd_buf, ",");
		if (p) {
			high_temp_threshold = atoi(p + 1);
			*ret_len = at_set_htw(ret_buf, warning_enabled, high_temp_threshold);
		} else {
			*ret_len = sprintf((char *)ret_buf, "%s\n", "+THRESHOLD:ERROR");
		}

	/* AT+HTW */
	} else if (strcmp((char *)cmd_buf, AT_HIGH_TEMP_WARNING_Q) == 0) {
		*ret_len = at_get_htw(ret_buf);

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

	/* AT+LOWTEMPCAL=x,x */
	} else if (strncmp((char *)cmd_buf, AT_LOW_TEMP_CAL, strlen(AT_LOW_TEMP_CAL)) == 0) {
		float R_low, t_low;

		p = cmd_buf + strlen(AT_LOW_TEMP_CAL);
		R_low =  atof(p);

		p = strstr((const char *)cmd_buf, ",");
		if (p)
			t_low = atof(p + 1);
		else
			t_low = 0;

		*ret_len = at_set_low_temp_cal(ret_buf, R_low, t_low);

	/* AT+LOWTEMPCAL */
	} else if (strcmp((char *)cmd_buf, AT_LOW_TEMP_CAL_Q) == 0) {
		*ret_len = at_get_low_temp_cal(ret_buf);

	/* AT+HIGHTEMPCAL=x,x */
	} else if (strncmp((char *)cmd_buf, AT_HIGH_TEMP_CAL, strlen(AT_HIGH_TEMP_CAL)) == 0) {
		float R_high, t_high;

		p = cmd_buf + strlen(AT_HIGH_TEMP_CAL);
		R_high =  atof(p);

		p = strstr((const char *)cmd_buf, ",");
		if (p)
			t_high = atof(p + 1);
		else
			t_high = 0;

		*ret_len = at_set_high_temp_cal(ret_buf, R_high, t_high);

	/* AT+HIGHTEMPCAL */
	} else if (strcmp((char *)cmd_buf, AT_HIGH_TEMP_CAL_Q) == 0) {
		*ret_len = at_get_high_temp_cal(ret_buf);

	/* AT+TEMPCAL=x,x */
	} else if (strncmp((char *)cmd_buf, AT_TEMP_CAL, strlen(AT_TEMP_CAL)) == 0) {
		float B_delta, R25_delta;

		p = cmd_buf + strlen(AT_TEMP_CAL);
		B_delta =  atof(p);

		p = strstr(cmd_buf, ",");
		if (p)
			R25_delta = atof(p + 1);
		else
			R25_delta = 0;

		*ret_len = at_set_temp_cal(ret_buf, B_delta, R25_delta);

	/* AT+TEMPCAL */
	} else if (strcmp((char *)cmd_buf, AT_TEMP_CAL_Q) == 0) {
		*ret_len = at_get_temp_cal(ret_buf);

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
		uint8 contrast;

		p = cmd_buf + strlen(AT_OLED_CONTRAST);
		contrast =  atoi(p);

		*ret_len = at_set_oled9639_contrast(ret_buf, contrast);

	/* AT+SERASE */
	} else if (strcmp((char *)cmd_buf, AT_S_ERASE) == 0) {
		*ret_len = at_serase(ret_buf);

	/* AT+SRESET */
	} else if (strcmp((char *)cmd_buf, AT_S_RESET) == 0) {
		*ret_len = at_sreset(ret_buf);

	/* AT+SINFO */
	} else if (strcmp((char *)cmd_buf, AT_S_INFO) == 0) {
		*ret_len = at_sinfo(ret_buf);

	/* AT+SRESTORE */
	} else if (strcmp((char *)cmd_buf, AT_S_RESTORE) == 0) {
		*ret_len = at_srestore(ret_buf);

	/* AT+STEST */
	} else if (strcmp((char *)cmd_buf, AT_S_TEST) == 0) {
		*ret_len = at_stest(ret_buf);

	} else {
		at_help();
	}

	return;

}
