
#ifndef __THER_WECHAT_SERVICE_H__
#define __THER_WECHAT_SERVICE_H__

// WECHAT Profile Service UUID
#define THER_WECHAT_UUID 0xFEE7

#define THER_WECHAT_SERVICE 0x00000001

#define WECHAT_INDICATE_VALUE_POS 2
#define WECHAT_INDICATE_CHAT_CONFIG_POS 3

enum {
	THER_WECHAT_SET_INDICATE,
	THER_WECHAT_DATA_WRITE,
	THER_WECHAT_DATE_READ,
};

bStatus_t ther_add_wechat_service(uint32 services, void (*handle_wechat_event)(uint8 event, uint8 *data, uint8 *len));
bStatus_t ther_wechat_send_indicate(uint16 connHandle, uint16 handle, uint8 *data, uint8 len, uint8 taskId );

#endif

