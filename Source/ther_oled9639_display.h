
#ifndef __THER_OLED9639_DISPLAY_H__
#define __THER_OLED9639_DISPLAY_H__

enum {
	OLED_DISPLAY_PICTURE1,
	OLED_DISPLAY_PICTURE2,
	OLED_DISPLAY_MAX_PICTURE,

	OLED_DISPLAY_OFF,
	OLED_DISPLAY_WELCOME,
	OLED_DISPLAY_GOODBYE,
};

enum {
	LINK_ON = 0,
	LINK_OFF,
};

enum {
	OLED_CONTENT_TIME = 0,
	OLED_CONTENT_LINK,
	OLED_CONTENT_BATT,
	OLED_CONTENT_TEMP,
	OLED_CONTENT_DUMMY_TEMP,
};

void oled_display_init(void);
void oled_show_welcome(void);
void oled_show_goodbye(void);
void oled_show_first_picture(unsigned short time, unsigned char link,
						unsigned char batt_level, unsigned short temp);
void oled_update_first_picture(unsigned char type, unsigned short val);
void oled_show_second_picture(void);
void oled_clear_screen(void);
void oled_power_on(void);
void oled_power_off(void);

#endif

