
#ifndef __THER_COMM_H__
#define __THER_COMM_H__

void ther_handle_gatt_msg(gattMsgEvent_t *msg);
void ther_send_temp_indicate(uint16 connect_handle, unsigned char task_id, unsigned short temp);
void ther_send_temp_notify(uint16 connect_handle, unsigned short temp);

#endif

