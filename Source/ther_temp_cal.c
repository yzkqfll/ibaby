
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "math.h"

#include "ther_uart.h"
#include "ther_adc.h"
#include "ther_temp_cal.h"

#define MODULE "[TEMP CAL] "

float temp_cal_get_res_by_adc(unsigned char channel, unsigned short adc_val)
{
	float Rsensor = 0;

	if (channel == HAL_ADC_CHANNEL_0) {
		/*
		 * From the Schematics
		 *
		 * 		(Vsensor * (Rsensor / (Rsensor + R9)) - Vsensor * (R50 / (R49 + R50))) * 5 = Vsensor * (adc / 8192)
		 * 	=>	Rsensor = R9 / (1 / (adc / (5 * 8192) + R50 / (R49 + R50)) - 1)
		 */
		Rsensor = 56.0 / (1.0 / ((float)adc_val / 40960.0 + 56.0 / (56.0 + 76.8)) - 1);

	} else if (channel == HAL_ADC_CHANNEL_1) {
		/*
		 * From the Schematics
		 *
		 * 		R9 / Rsensor = Vr9 / Vsensor       R9 = 56K, Vr9 = (Vref / 8192) * (8192 - adc), Vsensor = (Vref / 8192) * adc
		 * => 	Rsensor = 56K * adc / (8192 - adc)
		 */
		Rsensor = 56.0 * (float)adc_val / (8192.0 - (float)adc_val);
	}

	return Rsensor;
}

/*
 * 377 => 37.7 celsius
 */
unsigned short temp_cal_get_temp_by_res(float res)
{
	float temp;

	/*
	 * From Sensor SPEC:
	 *
	 * 3950.0 = ln(R25/Rsensor) / (1 / (25 + 273.15) - 1 / (temp + 273.15))
	 * 	R25 is the resistance in 25 du
	 *
	 * temp = 1 / (1 / (25 + 273.15) - ln(Rt25/Rsensor) / 3950)) - 273.15
	 */
	temp = 1.0 / (1.0 / (25.0 + 273.15) - log(100.0 / res) / 3950) - 273.15;

	temp = temp * 10.0;

	return (unsigned short)temp;
}


