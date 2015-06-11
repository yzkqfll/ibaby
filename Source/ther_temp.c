
#include "Comdef.h"
#include "OSAL.h"
#include "hal_drivers.h"

#include "hal_board.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_adc.h"
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

#define HIGH_PRESISION_TEMP_MIN 290
#define HIGH_PRESISION_TEMP_MAX 450

struct ther_temp {
	unsigned char presision_used;

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

/*
 *  channel 0(AIN0) is high presision
 *  channel 1(AIN1) is low presision
 */
static unsigned short ther_get_temp(unsigned char presision)
{
	unsigned short adc_val, temp;
	float sensor_res;
	unsigned char channel;

	if (presision == HIGH_PRESISION) {
		channel = HAL_ADC_CHANNEL_0;
	} else {
		channel = HAL_ADC_CHANNEL_1;
	}

	adc_val = read_adc(channel, HAL_ADC_RESOLUTION_14, HAL_ADC_REF_AIN7);

	sensor_res = temp_cal_get_res_by_adc(presision, adc_val);
	temp = temp_cal_get_temp_by_res(sensor_res);

	print(LOG_DBG, MODULE "ch %d adc %d, Rsensor %f, temp %d\r\n",
			channel, adc_val, sensor_res, temp);

	return temp;
}

void ther_temp_power_on(void)
{
	enable_ldo();
}

/*
 * return value: 377 => 37.7 du
 */
unsigned short ther_get_current_temp(void)
{
	struct ther_temp *t = &ther_temp;
	unsigned short temp; /* 377 => 37.7 du */

	temp = ther_get_temp(t->presision_used);

	if ((t->presision_used == LOW_PRESISION) && (temp > HIGH_PRESISION_TEMP_MIN) && (temp < HIGH_PRESISION_TEMP_MAX)) {
		print(LOG_INFO, MODULE "change to high presision\r\n");
		t->presision_used = HIGH_PRESISION;

	} else if ((t->presision_used == HIGH_PRESISION) && (temp < HIGH_PRESISION_TEMP_MIN || temp > HIGH_PRESISION_TEMP_MAX)) {
		print(LOG_INFO, MODULE "change to low presision\r\n");
		t->presision_used = LOW_PRESISION;
	}

	disable_ldo();

	return temp;
}


void ther_temp_init(void)
{
	struct ther_temp *t = &ther_temp;

	print(LOG_INFO, MODULE "temp init\r\n");

	t->presision_used = LOW_PRESISION;

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
}
