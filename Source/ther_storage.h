
#ifndef __THER_STORAGE_H__
#define __THER_STORAGE_H__

bool ther_storage_test(void);
bool storage_write_zero_cal(unsigned short hw_adc0, short delta);
bool storage_read_zero_cal(short *delta);
bool storage_save_temp(uint8 *data, uint16 len);
void storage_drain_temp(void);
void storage_reset(void);
void storage_restore_temp(uint8 **buf, uint16 *len);
void storage_show_info(void);
void ther_storage_init(void);
void ther_storage_exit(void);

bool storage_is_formated(void);
bool storage_format(void);
bool storage_erase(void);

#endif

