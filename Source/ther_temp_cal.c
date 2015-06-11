
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"
#include "math.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_temp_cal.h"

#define MODULE "[TEMP CAL] "

float temp_cal_get_res_by_adc(unsigned char channel, unsigned short adc_val)
{
	float sensor_resistance;

	if (channel == HIGH_PRESISION) {
		/*
		 * From the Schematics
		 *
		 * 		(Vsensor * (Rsensor / (Rsensor + R9)) - Vsensor * (R50 / (R49 + R50))) * 5 = Vsensor * (adc / 8192)
		 * 	=>	Rsensor = R9 / (1 / (adc / (5 * 8192) + R50 / (R49 + R50)) - 1)
		 */
		sensor_resistance = 56.0 / (1.0 / ((float)adc_val / 40960.0 + 56.0 / (56.0 + 76.8)) - 1);
	} else {
		/*
		 * From the Schematics
		 *
		 * 		R9 / Rsensor = Vr9 / Vsensor
		 * =>	56 / Rsensor = (8192 - adc) / adc
		 * => 	Rsensor = 56 * adc / (8192 - adc)
		 */
		sensor_resistance = 56.0 * (float)adc_val / (8192.0 - (float)adc_val);
	}

	return sensor_resistance;
}

/*
 * 377 => 37.7 du
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


