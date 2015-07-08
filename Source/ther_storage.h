
#ifndef __THER_STORAGE_H__
#define __THER_STORAGE_H__

bool ther_storage_test(void);
bool storage_write_zero_cal(unsigned short hw_adc0, short delta);
bool storage_read_zero_cal(short *delta);
bool storage_erase(void);
void ther_storage_init(void);
#endif

