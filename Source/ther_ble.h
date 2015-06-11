
#ifndef __THER_BLE_H__
#define __THER_BLE_H__

/*
 * ble event
 */
#define BLE_GATT_ACCESS_EVENT 0x10

/*
 * GATT access type
 */
enum {
	GATT_TEMP_IND_ENABLED = 0,
	GATT_TEMP_IND_DISABLED,

	GATT_IMEAS_NOTI_ENABLED,
	GATT_IMEAS_NOTI_DISABLED,

	GATT_INTERVAL_IND_ENABLED,
	GATT_INTERVAL_IND_DISABLED,

	GATT_UNKNOWN,
};

struct ble_gatt_access_msg {
  osal_event_hdr_t hdr;
  unsigned char type;
};

#define BLE_STATUS_CHANGE_EVENT 0x30

enum {
	BLE_DISCONNECT = 0,
	BLE_CONNECT,
};

struct ble_status_change_msg {
  osal_event_hdr_t hdr;
  unsigned char type;
};

unsigned char ther_ble_init(uint8 task_id);

void ble_start_advertise(void);
unsigned short ble_get_gap_handle(void);

#endif

