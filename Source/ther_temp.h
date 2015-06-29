
#ifndef __THER_TEMP_H__
#define __THER_TEMP_H__

enum {
	TEMP_STAGE_SETUP,
	TEMP_STAGE_MEASURE,
};

void ther_temp_power_on(void);
void ther_temp_power_off(void);

unsigned short ther_get_temp(void);
unsigned short ther_get_adc(unsigned char ch);
unsigned short ther_get_hw_adc(unsigned char ch);

void ther_set_adc0_delta(short delta);
short ther_get_adc0_delta(void);
void ther_set_B_delta(float delta);
float ther_get_B_delta(void);
void ther_set_R25_delta(float delta);
float ther_get_R25_delta(void);

float ther_get_Rt(unsigned char ch);
unsigned short ther_get_ch_temp(unsigned char presision);

void ther_temp_init(void);

#endif

