
#ifndef __THER_STORAGE_H__
#define __THER_STORAGE_H__

void ther_storage_test(void);
bool ther_write_zero_cal_info(unsigned short hw_adc0, short delta);
bool ther_read_zero_cal_info(short *delta);

#endif

