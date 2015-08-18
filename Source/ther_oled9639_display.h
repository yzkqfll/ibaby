
#ifndef __THER_OLED9639_DISPLAY_H__
#define __THER_OLED9639_DISPLAY_H__

enum {
	OLED_PICTURE1,
	OLED_PICTURE2,
	OLED_PICTURE_MAX,

	OLED_PICTURE_NONE,
	OLED_PICTURE_WELCOME,
	OLED_PICTURE_GOODBYE,
};

enum {
	BATT_FULL = 0,
	BATT_ALMOST_FULL, /* 3/4 */
	BATT_HALF,
	BATT_ALMOST_EMPTY,
	BATT_EMPTY,
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
	OLED_CONTENT_MAX_TEMP,
};

enum {
	OLED_EVENT_DISPLAY_ON,
	OLED_EVENT_TIME_TO_END,
};

struct display_param {
	bool ble_link;
	unsigned short temp;
	uint16 max_temp;
	unsigned short time;
	unsigned char batt_percentage;
};

void oled_display_init(unsigned char task_id, void (*event_report)(unsigned char event, unsigned short param));
void oled_display_exit(void);
void oled_display_state_machine(void);
void oled_display_power_off(void);
void oled_update_picture(uint8 type, uint16 val);
void oled_show_picture(uint8 picture, uint16 remain_ms, struct display_param *param);
void oled_show_next_picture(uint16 time_ms, struct display_param *param);

#endif

