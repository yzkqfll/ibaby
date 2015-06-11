
#ifndef __THER_TEMP_H__
#define __THER_TEMP_H__

enum {
	TEMP_STAGE_SETUP,
	TEMP_STAGE_MEASURE,
};

unsigned short ther_get_current_temp(void);
void ther_temp_power_on(void);
void ther_temp_init(void);

#endif

