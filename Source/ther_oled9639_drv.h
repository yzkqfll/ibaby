
#ifndef __THER_OLED9639_DRV_H__
#define __THER_OLED9639_DRV_H__


#define MAX_COL 96
#define MAX_ROW 39

#define MAX_PAGE 5 /* 8, 8, 8, 8, 7 = 39 lines */

void oled_drv_init(void);
void oled_drv_init_device(void);
void oled_drv_fill_screen(unsigned char val);
void oled_drv_write_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, unsigned char *data);
void oled_drv_fill_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, unsigned char data);
void oled_drv_power_off(void);
void oled_drv_power_on(void);
void oled_drv_display_off(void);
void oled_drv_display_on(void);

#endif

