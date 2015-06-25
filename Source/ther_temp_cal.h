
#ifndef __THER_TEMP_CAL_H__
#define __THER_TEMP_CAL_H__

float temp_cal_get_res_by_ch0(unsigned short adc_val);
float temp_cal_get_res_by_ch1(unsigned short adc_val);
unsigned short temp_cal_get_temp_by_res(float res);

#endif

