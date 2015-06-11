
#ifndef __THER_BUTTON_H__
#define __THER_BUTTON_H__


/*
 * user button event
 */
#define USER_BUTTON_EVENT 0x20

/*
 * button type
 */
enum {
	SHORT_PRESS = 0,
	LONG_PRESS,

	BUTTON_UNKNOWN,
};

struct button_msg {
  osal_event_hdr_t hdr; //!< BLE_MSG_EVENT and status
  unsigned char type;
};

void ther_measure_button_time(void);
void ther_button_init(unsigned char task_id);

#endif

