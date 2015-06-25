
#ifndef __THER_TEMP_H__
#define __THER_TEMP_H__

enum {
	TEMP_STAGE_SETUP,
	TEMP_STAGE_MEASURE,
};

unsigned short ther_auto_get_temp(void);
unsigned short ther_get_temp(unsigned char presision);
unsigned short ther_get_adc(unsigned char channel);
void ther_temp_power_on(void);
void ther_temp_power_off(void);
void ther_temp_init(void);

#endif

