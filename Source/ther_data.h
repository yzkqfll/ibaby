
#ifndef __THER_DATA_H__
#define __THER_DATA_H__

#include "gatt.h"
#include "OSAL_Clock.h"

struct temp_data {
	uint8 flag;
	uint32 temp;

	uint16 year;    // 2000+
	uint8 month;    // 0-11
	uint8 day;      // 0-30
	uint8 hour;     // 0-23
	uint8 minutes;  // 0-59
	uint8 seconds;  // 0-59

	uint8 type;
};

void ther_handle_gatt_msg(gattMsgEvent_t *msg);
void ther_save_temp_to_local(unsigned short temp);
void ther_send_temp_indicate(unsigned char task_id, unsigned short temp);
void ther_send_temp_notify(unsigned short temp);
void ther_send_history_temp(unsigned char task_id, uint8 *data, uint8 len);

#endif

