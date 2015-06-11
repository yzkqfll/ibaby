
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_adc.h"

#define MODULE "[ADC] "

#define HAL_ADC_EOC         0x80    /* End of Conversion bit */
#define HAL_ADC_START       0x40    /* Starts Conversion */

#define HAL_ADC_DEC_064     0x00    /* Decimate by 64 : 8-bit resolution */
#define HAL_ADC_DEC_128     0x10    /* Decimate by 128 : 10-bit resolution */
#define HAL_ADC_DEC_256     0x20    /* Decimate by 256 : 12-bit resolution */
#define HAL_ADC_DEC_512     0x30    /* Decimate by 512 : 14-bit resolution */
#define HAL_ADC_DEC_BITS    0x30    /* Bits [5:4] */

unsigned short read_adc(unsigned char channel, unsigned char resolution, unsigned char vref)
{
	int16  reading = 0;
	uint8   i, resbits;
	uint8  adcChannel = 1;

	/*
	* If Analog input channel is AIN0..AIN7, make sure corresponing P0 I/O pin is enabled.  The code
	* does NOT disable the pin at the end of this function.  I think it is better to leave the pin
	* enabled because the results will be more accurate.  Because of the inherent capacitance on the
	* pin, it takes time for the voltage on the pin to charge up to its steady-state level.  If
	* HalAdcRead() has to turn on the pin for every conversion, the results may show a lower voltage
	* than actuality because the pin did not have time to fully charge.
	*/
	if (channel <= HAL_ADC_CHANNEL_7)
	{
		for (i=0; i < channel; i++)
		{
			adcChannel <<= 1;
		}

		/* Enable channel */
		ADCCFG |= adcChannel;
	}

	/* Convert resolution to decimation rate */
	switch (resolution)
	{
		case HAL_ADC_RESOLUTION_8:
			resbits = HAL_ADC_DEC_064;
			break;
		case HAL_ADC_RESOLUTION_10:
			resbits = HAL_ADC_DEC_128;
			break;
		case HAL_ADC_RESOLUTION_12:
			resbits = HAL_ADC_DEC_256;
			break;
		case HAL_ADC_RESOLUTION_14:
		default:
			resbits = HAL_ADC_DEC_512;
			break;
	}

	/* writing to this register starts the extra conversion */
	ADCCON3 = channel | resbits | vref;

	/* Wait for the conversion to be done */
	while (!(ADCCON1 & HAL_ADC_EOC));

	/* Disable channel after done conversion */
	if (channel <= HAL_ADC_CHANNEL_7)
		ADCCFG &= (adcChannel ^ 0xFF);

	/* Read the result */
	reading = (int16) (ADCL);
	reading |= (int16) (ADCH << 8);

	/* Treat small negative as 0 */
	if (reading < 0)
		reading = 0;

	switch (resolution)
	{
		case HAL_ADC_RESOLUTION_8:
			reading >>= 8;
			break;
		case HAL_ADC_RESOLUTION_10:
			reading >>= 6;
			break;
		case HAL_ADC_RESOLUTION_12:
			reading >>= 4;
			break;
		case HAL_ADC_RESOLUTION_14:
		default:
			reading >>= 2;
			break;
	}
	return ((uint16)reading);
}
