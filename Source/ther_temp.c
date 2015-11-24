/*
 * THER TEMP
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

#include "Comdef.h"
#include "OSAL.h"
#include "hal_drivers.h"
#include "math.h"

#include "config.h"

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_adc.h"
#include "ther_temp.h"
#include "ther_storage.h"
#include "ther_ble.h"
#include "ther_port.h"

#define MODULE "[TEMP   ] "

/*
 * CH0 26.88~46.6
 */
#define TEMP_MARGIN 2.0 /* 2 celsius */
#define CH0_TEMP_MIN 29.0
#define CH0_TEMP_MAX 44.0

struct ther_temp {
	unsigned char channel;

	short adc0_delta;
	float adc1_k;

	float B_delta;
	float R25_delta;

};
static struct ther_temp ther_temp;


static void enable_ldo(void)
{
	P0_LDO_EN_PIN = 1;
}

static void disable_ldo(void)
{
	P0_LDO_EN_PIN = 0;
}

static float calculate_Rt_by_adc0(unsigned short adc)
{
	float Rt = 0;

	/*
	 * From the Schematics
	 *
	 * 		(Vsensor * (Rt / (Rt + R9)) - Vsensor * (R50 / (R49 + R50))) * 5 = Vsensor * (adc / 8192)
	 * 	=>	Rt = R9 / (1 / (adc / (5 * 8192) + R50 / (R49 + R50)) - 1)
	 */
	Rt = 56.0 / (1.0 / ((float)adc / 40960.0 + 56.0 / (56.0 + 76.8)) - 1);

	return Rt;
}

static float calculate_Rt_by_adc1(unsigned short adc)
{
	float Rt = 0;

	/*
	 * From the Schematics
	 *
	 * 		R9 / Rt = Vr9 / Vsensor       R9 = 56K, Vr9 = (Vref / 8192) * (8192 - adc), Vsensor = (Vref / 8192) * adc
	 * => 	Rt = 56K * adc / (8192 - adc)
	 */
	Rt = 56.0 * (float)adc / (8192.0 - (float)adc);

	return Rt;
}

/*
 *
 */
static float calculate_temp_by_Rt(float Rt, float B_delta, float R25_delta)
{
	float temp;
	float B = 3950 + B_delta;
	float R25 = 100.0 + R25_delta;

	/*
	 * From Sensor SPEC:
	 *
	 * 3950.0 = ln(R25/Rt) / (1 / (25 + 273.15) - 1 / (temp + 273.15))
	 * 	R25 is the resistance in 25 celsius
	 *
	 * temp = 1 / (1 / (25 + 273.15) - ln(Rt25/Rt) / 3950)) - 273.15
	 */
	temp = 1.0 / (1.0 / (25.0 + 273.15) - log(R25 / Rt) / B) - 273.15;

	return temp;
}

void ther_temp_power_on(void)
{
	enable_ldo();
}

void ther_temp_power_off(void)
{
	disable_ldo();
}

void ther_set_adc0_delta(short delta)
{
	struct ther_temp *t = &ther_temp;

	storage_write_zero_cal(0, delta, 0);

	t->adc0_delta = delta;
}

short ther_get_adc0_delta(void)
{
	struct ther_temp *t = &ther_temp;

	return t->adc0_delta;
}

void ther_set_adc1_k(float k)
{
	struct ther_temp *t = &ther_temp;

	storage_write_zero_cal(0, 0, k);

	t->adc1_k = k;
}

float ther_get_adc1_k(void)
{
	struct ther_temp *t = &ther_temp;

	return t->adc1_k;
}

void ther_set_temp_delta(float B_delta, float R25_delta)
{
	struct ther_temp *t = &ther_temp;

	t->B_delta = B_delta;
	t->R25_delta = R25_delta;

	storage_write_temp_cal(B_delta, R25_delta);
}

void ther_get_temp_cal(float *B_delta, float *R25_delta)
{
	struct ther_temp *t = &ther_temp;

	*B_delta = t->B_delta;
	*R25_delta = t->R25_delta;
}

#define SAMPLING_NUM 40

static unsigned short cal_average(unsigned short val[], uint8 num)
{
	unsigned char i, j;
	unsigned char max_index;
	uint32 tmp, sum = 0;

/*
	for (i = 0; i < num; i++) {
		print(LOG_DBG, MODULE "%d: %d\n", i, val[i]);
	}
*/

	if (num < 3)
		return val[num / 2];

/*	for (i = 0; i < num; i++) {
		max_index = i;
		for (j = i + 1; j < num; j++) {
			if (val[max_index] < val[j]) {
				max_index = j;
			}
		}
		tmp = val[i];
		val[i] = val[max_index];
		val[max_index] = tmp;
	}*/

	for (i = 1; i < num - 1; i++) {
		sum += val[i];
	}
	return sum / (num - 2);
}

unsigned short ther_get_hw_adc(unsigned char ch, uint8 from)
{
	unsigned short ave_adc;
	uint8 times;

	if (from == FROM_AT_CMD && ch == HAL_ADC_CHANNEL_0)
		times = SAMPLING_NUM;
	else
		times = 1;

	if (ch <= HAL_ADC_CHANNEL_7) {
		uint8 i;
		unsigned short adc[SAMPLING_NUM] = {0};

		for (i = 0; i < times; i++)
			adc[i] = read_adc(ch, HAL_ADC_RESOLUTION_14, HAL_ADC_REF_AIN7);

		ave_adc = cal_average(adc, times);
	}

	return ave_adc;
}

unsigned short ther_get_adc(unsigned char ch, uint8 from)
{
	struct ther_temp *t = &ther_temp;
	unsigned short adc;

	adc = ther_get_hw_adc(ch, from);

	if (ch == HAL_ADC_CHANNEL_0)
		adc += t->adc0_delta;
	else if (ch == HAL_ADC_CHANNEL_1)
		adc *= t->adc1_k;

	return adc;
}

float ther_get_Rt(unsigned char ch, uint8 from)
{
	unsigned short adc;
	float Rt = 0;

	adc = ther_get_adc(ch, from);

	/* ADC -> Rt */
	if (ch == HAL_ADC_CHANNEL_0) {
		Rt = calculate_Rt_by_adc0(adc);
	} else if (ch == HAL_ADC_CHANNEL_1) {
		Rt = calculate_Rt_by_adc1(adc);
	}

	return Rt;
}

float ther_get_temp(unsigned char ch, uint8 from)
{
	struct ther_temp *t = &ther_temp;
	unsigned short adc;
	float Rt, temp;

	adc = ther_get_adc(ch, from);

	/* ADC -> Rt */
	if (ch == HAL_ADC_CHANNEL_0) {
		Rt = calculate_Rt_by_adc0(adc);
	} else if (ch == HAL_ADC_CHANNEL_1) {
		Rt = calculate_Rt_by_adc1(adc);
	}

	/* Rt -> temp */
	temp = calculate_temp_by_Rt(Rt, t->B_delta, t->R25_delta);

	if (from == FROM_CUSTOM) {
		print(LOG_DBG, MODULE "ch %d adc %d, Rt %f, temp %.2f\n",
				ch, adc, Rt, temp);
	}

	return temp;
}

/*
 * For thermometer
 */
float ther_read_temp(void)
{
	struct ther_temp *t = &ther_temp;
	float temp;

	temp = ther_get_temp(t->channel, FROM_CUSTOM);

	if (t->channel == HAL_ADC_CHANNEL_1) {
		if (temp > CH0_TEMP_MIN + TEMP_MARGIN && temp < CH0_TEMP_MAX - TEMP_MARGIN)
			t->channel = HAL_ADC_CHANNEL_0;

	} else if (t->channel == HAL_ADC_CHANNEL_0) {
		if (temp < CH0_TEMP_MIN || temp > CH0_TEMP_MAX)
			t->channel = HAL_ADC_CHANNEL_1;
	}

	return temp;
}

void ther_temp_init(void)
{
	struct ther_temp *t = &ther_temp;

	print(LOG_INFO, MODULE "temp init\n");

	t->channel = HAL_ADC_CHANNEL_1;

	storage_read_zero_cal(&t->adc0_delta, &t->adc1_k);
	print(LOG_INFO, MODULE "ADC0 delta is %d, ADC1 k is %.2f\n", t->adc0_delta, t->adc1_k);

	storage_read_temp_cal(&t->B_delta, &t->R25_delta);
	print(LOG_INFO, MODULE "B_delta is %d, R25_delta is %d\n", t->B_delta, t->R25_delta);
}
