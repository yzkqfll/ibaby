
#ifndef __THER_BLE_H__
#define __THER_BLE_H__


#define BLE_STATUS_CHANGE_EVENT 0x30

enum {
	BLE_DISCONNECT = 0,
	BLE_CONNECT,
};

struct ble_status_change_msg {
  osal_event_hdr_t hdr;
  unsigned char type;
};


unsigned char ther_ble_init(uint8 task_id, void (*handle_ts_event)(unsigned char event),
		void (*handle_ps_event)(unsigned char event, unsigned char *data, unsigned char *len));
void ther_ble_exit(void);

void ble_start_advertise(void);
unsigned short ble_get_gap_handle(void);

#endif

