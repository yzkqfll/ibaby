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

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_adc.h"
#include "ther_temp.h"
#include "ther_storage.h"


#define MODULE "[THER TEMP] "

/* p2.3: ldo enable */
#define LDO_ENABLE_PIN P2_3
#define LDO_ENABLE_BIT 3

/* p0.7: Vref */
#define ADC_REF_VOLTAGE_PIN P0_7
#define ADC_REF_VOLTAGE_BIT 7

/* p0.0: high presision temp */
#define ADC_HIGH_PRECISION_PIN P0_0
#define ADC_HIGH_RRECISION_BIT 0

/* p0.1: low presision temp */
#define ADC_LOW_PRECISION_PIN P0_1
#define ADC_LOW_PRECISION_BIT 1

#define TEMP_MARGIN 20 /* 2 celsius */
#define CH0_TEMP_MIN 290
#define CH0_TEMP_MAX 450

struct ther_temp {
	unsigned char channel;

	short adc0_delta;

	float B_delta;
	float R25_delta;

	/* save power */
	unsigned short low_presision_pre_adc;
	unsigned short low_presision_pre_temp;
};
static struct ther_temp ther_temp;


static void enable_ldo(void)
{
	LDO_ENABLE_PIN = 1;
}

static void disable_ldo(void)
{
	LDO_ENABLE_PIN = 0;
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
 * 377 => 37.7 celsius
 */
static unsigned short calculate_temp_by_Rt(float Rt, float B_delta, float R25_delta)
{
	float temp;
	float B = 3950 + B_delta;
	float R25 = 100.0 + R25_delta;

	/*
	 * From Sensor SPEC:
	 *
	 * 3950.0 = ln(R25/Rt) / (1 / (25 + 273.15) - 1 / (temp + 273.15))
	 * 	R25 is the resistance in 25 du
	 *
	 * temp = 1 / (1 / (25 + 273.15) - ln(Rt25/Rt) / 3950)) - 273.15
	 */
	temp = 1.0 / (1.0 / (25.0 + 273.15) - log(R25 / Rt) / B) - 273.15;
	temp += 0.05;

	temp = temp * 10.0;

	return (unsigned short)temp;
}

void ther_temp_power_on(void)
{
	enable_ldo();
}

void ther_temp_power_off(void)
{
	disable_ldo();
}

unsigned short ther_get_hw_adc(unsigned char ch)
{
	unsigned short adc = 0;

	if (ch <= HAL_ADC_CHANNEL_7)
		adc = read_adc(ch, HAL_ADC_RESOLUTION_14, HAL_ADC_REF_AIN7);

	return adc;
}

unsigned short ther_get_adc(unsigned char ch)
{
	struct ther_temp *t = &ther_temp;
	unsigned short adc;

	adc = ther_get_hw_adc(ch);

	if (ch == HAL_ADC_CHANNEL_0)
		adc += t->adc0_delta;
	else
		adc += 0;

	return adc;
}

void ther_set_adc0_delta(short delta)
{
	struct ther_temp *t = &ther_temp;

	ther_write_zero_cal_info(0, delta);

	t->adc0_delta = delta;
}

short ther_get_adc0_delta(void)
{
	struct ther_temp *t = &ther_temp;

	return t->adc0_delta;
}

void ther_set_B_delta(float delta)
{
	struct ther_temp *t = &ther_temp;

	t->B_delta = delta;
}

float ther_get_B_delta(void)
{
	struct ther_temp *t = &ther_temp;

	return t->B_delta;
}

void ther_set_R25_delta(float delta)
{
	struct ther_temp *t = &ther_temp;

	t->R25_delta = delta;
}

float ther_get_R25_delta(void)
{
	struct ther_temp *t = &ther_temp;

	return t->R25_delta;
}


float ther_get_Rt(unsigned char ch)
{
	unsigned short adc;
	float Rt = 0;

	adc = ther_get_adc(ch);

	/* ADC -> Rt */
	if (ch == HAL_ADC_CHANNEL_0) {
		Rt = calculate_Rt_by_adc0(adc);
	} else if (ch == HAL_ADC_CHANNEL_1) {
		Rt = calculate_Rt_by_adc1(adc);
	}

	return Rt;
}

unsigned short ther_get_ch_temp(unsigned char ch)
{
	struct ther_temp *t = &ther_temp;
	unsigned short temp;
	float Rt;

	Rt = ther_get_Rt(ch);

	/* Rt -> temp */
	temp = calculate_temp_by_Rt(Rt, t->B_delta, t->R25_delta);

	return temp;
}

static unsigned short ther_get_ch_temp_print(unsigned char ch)
{
	struct ther_temp *t = &ther_temp;
	unsigned short adc, temp;
	float Rt;

	adc = ther_get_adc(ch);

	/* ADC -> Rt */
	if (ch == HAL_ADC_CHANNEL_0) {
		Rt = calculate_Rt_by_adc0(adc);
	} else if (ch == HAL_ADC_CHANNEL_1) {
		Rt = calculate_Rt_by_adc1(adc);
	}

	/* Rt -> temp */
	temp = calculate_temp_by_Rt(Rt, t->B_delta, t->R25_delta);

	print(LOG_DBG, MODULE "ch %d adc %d, Rt %f, temp %d\n",
			ch, adc, Rt, temp);

	return temp;
}

/*
 * return value: 377 => 37.7 Celsius
 */
unsigned short ther_get_temp(void)
{
	struct ther_temp *t = &ther_temp;
	unsigned short temp; /* 377 => 37.7 Celsius */

	t->channel = HAL_ADC_CHANNEL_0;
	temp = ther_get_ch_temp_print(t->channel);

	if ((t->channel == HAL_ADC_CHANNEL_1) && (temp > CH0_TEMP_MIN + TEMP_MARGIN)) {
		t->channel = HAL_ADC_CHANNEL_0;

	} else if ((t->channel == HAL_ADC_CHANNEL_0) && (temp < CH0_TEMP_MIN - TEMP_MARGIN)) {
		t->channel = HAL_ADC_CHANNEL_1;
	}

	return temp;
}

void ther_temp_init(void)
{
	struct ther_temp *t = &ther_temp;

	print(LOG_INFO, MODULE "temp init\n");

	t->channel = HAL_ADC_CHANNEL_1;

	ther_read_zero_cal_info(&t->adc0_delta);
	print(LOG_INFO, MODULE "ADC0 delta %d\n", t->adc0_delta);
}
