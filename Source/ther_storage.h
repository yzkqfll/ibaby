
#ifndef __THER_STORAGE_H__
#define __THER_STORAGE_H__

bool storage_test(void);
bool storage_write_zero_cal(unsigned short hw_adc0, short adc0_delta, float adc1_k);
bool storage_read_zero_cal(short *adc0_delta, float *adc1_k);
bool storage_write_low_temp_cal(float R_low, float t_low);
bool storage_read_low_temp_cal(float *R_low, float *t_low);
bool storage_write_high_temp_cal(float R_high, float t_high);
bool storage_read_high_temp_cal(float *R_high, float *t_high);
bool storage_write_temp_cal(float B_delta, float R25_delta);
bool storage_read_temp_cal(float *B_delta, float *R25_delta);
bool storage_save_temp(uint8 *data, uint16 len);
void storage_drain_temp(void);
void storage_reset(void);
void storage_restore_temp(uint8 **buf, uint16 *len);
void storage_show_info(void);
void ther_storage_init(void);
void ther_storage_exit(void);

bool storage_write_high_temp_enabled(uint8 enabled);
bool storage_read_high_temp_enabled(uint8 *enabled);
bool storage_write_high_temp_threshold(int16 high_temp_threshold);
bool storage_read_high_temp_threshold(int16 *high_temp_threshold);

bool storage_is_formated(void);
bool storage_format(void);
bool storage_erase(void);

#endif

