
#ifndef __THER_TEMP_H__
#define __THER_TEMP_H__

enum {
	TEMP_STAGE_SETUP,
	TEMP_STAGE_MEASURE,
};

enum {
	FROM_AT_CMD = 0,
	FROM_CUSTOM,
};

void ther_temp_power_on(void);
void ther_temp_power_off(void);

float ther_read_temp(void);
unsigned short ther_get_adc(unsigned char ch, uint8 from);
unsigned short ther_get_hw_adc(unsigned char ch, uint8 from);

void ther_set_adc0_delta(short delta);
short ther_get_adc0_delta(void);
void ther_set_adc1_k(float k);
float ther_get_adc1_k(void);
void ther_set_temp_delta(float B_delta, float R25_delta);
void ther_get_temp_cal(float *B_delta, float *R25_delta);

float ther_get_Rt(unsigned char ch, uint8 from);
float ther_get_temp(unsigned char ch, uint8 from);

void ther_temp_init(void);

#endif

