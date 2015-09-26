
#ifndef __THER_WECHAT_H__
#define __THER_WECHAT_H__

enum {
	errorCodeUnpackAuthResp = 0x9990,
	errorCodeUnpackInitResp = 0x9991,
	errorCodeUnpackSendDataResp = 0x9992,
	errorCodeUnpackCtlCmdResp = 0x9993,
	errorCodeUnpackRecvDataPush = 0x9994,
	errorCodeUnpackSwitchViewPush = 0x9995,
	errorCodeUnpackSwitchBackgroundPush = 0x9996,
	errorCodeUnpackErrorDecode = 0x9997,
};

struct wx_header
{
	unsigned char bMagicNumber;
	unsigned char bVer;
	unsigned short nLength;
	unsigned short nCmdId;
	unsigned short nSeq;
};

struct auth_response {
	struct wx_header header;
};

void __wechat_send_auth(void);
void ther_wechat_handle_indicate_confirm(uint8 *data);
void ther_wechat_send_auth_req(void);
void ther_wechat_write(uint8 *data, uint8 *len);
void ther_wechat_init(uint8 task_id);
void ther_wechat_exit(void);

#endif

