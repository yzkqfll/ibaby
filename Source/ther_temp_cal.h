
#ifndef __THER_TEMP_CAL_H__
#define __THER_TEMP_CAL_H__

enum {
	HIGH_PRESISION = 0,
	LOW_PRESISION,
};

float temp_cal_get_res_by_adc(unsigned char channel, unsigned short adc_val);
unsigned short temp_cal_get_temp_by_res(float res);

#endif

