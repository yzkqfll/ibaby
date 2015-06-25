
#include "Comdef.h"
#include "OSAL.h"
#include "hal_drivers.h"

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_adc.h"
#include "ther_temp.h"
#include "ther_temp_cal.h"

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

unsigned short ther_get_adc(unsigned char channel)
{
	unsigned short adc = 0;

	if (channel < HAL_ADC_CHANNEL_2) {
		P0_6 = 1;
		adc = read_adc(channel, HAL_ADC_RESOLUTION_14, HAL_ADC_REF_AIN7);
		P0_6 = 0;
	}

	return adc;
}

/*
 *  channel 0(AIN0) is high presision
 *  channel 1(AIN1) is low presision
 */
unsigned short ther_get_temp(unsigned char ch)
{
	unsigned short adc_val, temp;
	float Rt;

	adc_val = ther_get_adc(ch);

	if (ch == HAL_ADC_CHANNEL_0) {
		Rt = temp_cal_get_res_by_ch0(adc_val);
	} else if (ch == HAL_ADC_CHANNEL_1) {
		Rt = temp_cal_get_res_by_ch1(adc_val);
	} else {
		return 0;
	}

	temp = temp_cal_get_temp_by_res(Rt);

	print(LOG_DBG, MODULE "ch %d adc %d, Rt %f, temp %d\n",
			ch, adc_val, Rt, temp);

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

/*
 * return value: 377 => 37.7 Celsius
 */
unsigned short ther_auto_get_temp(void)
{
	struct ther_temp *t = &ther_temp;
	unsigned short temp; /* 377 => 37.7 Celsius */

	temp = ther_get_temp(t->channel);

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

	/*
	 * init adc pins:
	 *   P2.3:  LDO enable pin
	 *   P0.7:  reference voltage
	 *   P0.0:  high resolution adc pin
	 *   P0.1:  low resolution adc pin
	 */

	/*
	 * P2.3: gpio, output
	 *
	 * P2.3 is mulplex with XOSC32K, so if we want to use it as gpio,
	 * we need set OSC32K_INSTALLED=FALSE in IAR options.
	 *
	 * see datasheet <7.8 32-kHz XOSC Input>
	 */
	P2SEL &= ~BV(1); /* P2.3 function select: GPIO */
	P2DIR |= BV(LDO_ENABLE_BIT); /* P2.3 as output */
//	P2INP &= ~BV(7); /* all port2 pins pull up */
//	P2INP |= BV(LDO_ENABLE_BIT); /* 3-state */

	/* P0.7, P0.0, P0.1: input, 3-state */
	P0SEL |= (BV(ADC_REF_VOLTAGE_BIT) | BV(ADC_HIGH_RRECISION_BIT) | BV(ADC_LOW_PRECISION_BIT));
	/* override P0SEL */
	ADCCFG |= (BV(ADC_REF_VOLTAGE_BIT) | BV(ADC_HIGH_RRECISION_BIT) | BV(ADC_LOW_PRECISION_BIT));

	P0DIR &= ~(BV(ADC_REF_VOLTAGE_BIT) | BV(ADC_HIGH_RRECISION_BIT) | BV(ADC_LOW_PRECISION_BIT));
	P0INP |= BV(ADC_REF_VOLTAGE_BIT) | BV(ADC_HIGH_RRECISION_BIT) | BV(ADC_LOW_PRECISION_BIT); /* 3-state */

	/* For Jerry test: P0.6 */
	P0DIR |= BV(6);
	P0SEL &= ~BV(6);
	P0_6 = 0;
}
