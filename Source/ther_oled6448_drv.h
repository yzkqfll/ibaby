
#ifndef __THER_OLED6448_DRV_H__
#define __THER_OLED6448_DRV_H__

#define OLED_IIC_ADDR 0x3C

#define BASE_COL 32

#define MAX_COL 64

#define MAX_ROW 48

#define MAX_PAGE 6 /* 8, 8, 8, 8, 8, 8 = 39 lines */

void oled_drv_init(void);
void oled_drv_exit(void);
void oled_open_iic(void);
void oled_close_iic(void);
void oled_drv_init_device(void);
void oled_drv_fill_screen(unsigned char val);
void oled_drv_write_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, const unsigned char *data);
void oled_drv_fill_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, unsigned char data);
void oled_drv_power_on_vdd(void);
void oled_drv_power_off_vdd(void);
void oled_drv_power_on_vcc(void);
void oled_drv_power_off_vcc(void);
void oled_drv_display_off(void);
void oled_drv_display_on(void);

void oled_drv_charge_pump_enable(void);
void oled_drv_charge_pump_disable(void);

void oled_drv_set_contrast(unsigned char contrast);
#endif

