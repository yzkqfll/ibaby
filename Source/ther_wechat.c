/*
 * THER WECHAT
 *
 * Copyright (c) 2015 by Leo Liu <59089403@qq.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 *
 * 2015/09/19 - Init version
 *              by Leo Liu <59089403@qq.com>
 *
 */

#include <string.h>

#include "bcomdef.h"
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "att.h"
#include "gatt.h"
#include "ther_uart.h"
#include "epb_MmBp.h"
#include "thermometer.h"
#include "ther_wechat.h"
#include "ther_wechat_service.h"
#include "ther_ble.h"

#define MODULE "[THER WC] "

#define WX_HEADER_MAGIC 0xFE

/*
 * AUTH
 */
#define DEVICE_TYPE "gh_4d6a769f76aa"
#define DEVICE_ID "WeChatBluetoothDevice"

#define PROTO_VERSION 0x010004
#define AUTH_PROTO 1

#define AUTH_METHOD EAM_macNoEncrypt
#define MD5_TYPE_AND_ID_LENGTH 0
#define CIPHER_TEXT_LENGTH 0

/*
 * INIT
 */
#define CHALLENAGE_LENGTH 4
static uint8 challeange[CHALLENAGE_LENGTH] = {0x11, 0x22, 0x33, 0x44};

enum {
	WECHAT_WAIT_INDICATE_ENABLE,

	WECHAT_SENDING_AUTH_REQUEST,
	WECHAT_WAIT_AUTH_RESPONSE,

	WECHAT_SENDING_INIT_REQUEST,
	WECHAT_WAIT_INIT_RESPONSE,

	WECHAT_OK,
};



#define BLE_TX_MAX_LEN (ATT_MTU_SIZE - 3)

struct weixin {
	uint8 task_id;

	uint8 state;
	uint16 seq;

	uint8 *auth_data;
	uint16 auth_len;
	uint16 auth_offset;

	uint8 *init_data;
	uint16 init_len;
	uint16 init_offset;

	uint8 *recv_data;
	uint16 recv_len;
	uint16 recv_offset;
};

static struct weixin *wx;

static struct weixin *get_wx(void) {
	return wx;
}

#define BigLittleSwap16(A)  ((((uint16_t)(A) & 0xff00) >> 8) | \
                            (((uint16_t)(A) & 0x00ff) << 8))


#define BigLittleSwap32(A)  ((((uint32_t)(A) & 0xff000000) >> 24) | \
                            (((uint32_t)(A) & 0x00ff0000) >> 8) | \
                            (((uint32_t)(A) & 0x0000ff00) << 8) | \
                            (((uint32_t)(A) & 0x000000ff) << 24))

static int checkCPUendian()
{
	union{
		unsigned long i;
		uint8_t s[4];
	}c;

	c.i = 0x12345678;
	return (0x12 == c.s[0]);
}

static unsigned long t_htonl(unsigned long h)
{
       return checkCPUendian() ? h : BigLittleSwap32(h);
}

static unsigned long t_ntohl(unsigned long n)
{

       return checkCPUendian() ? n : BigLittleSwap32(n);
}

static unsigned short htons(unsigned short h)
{
       return checkCPUendian() ? h : BigLittleSwap16(h);
}

static unsigned short ntohs(unsigned short n)
{
       return checkCPUendian() ? n : BigLittleSwap16(n);
}

static void __wechat_send_auth(void)
{
	struct weixin *wx = get_wx();
	uint8 tx_len;

	if (wx->auth_offset < wx->auth_len) {
		tx_len = (wx->auth_len - wx->auth_offset < BLE_TX_MAX_LEN) ? wx->auth_len - wx->auth_offset : BLE_TX_MAX_LEN;

		print(LOG_DBG, MODULE "AUTH: send req chunk: offset %d, len %d\n", wx->auth_offset, tx_len);
		ther_wechat_send_indicate(ble_get_gap_handle(),
								WECHAT_INDICATE_VALUE_POS,
								wx->auth_data + wx->auth_offset,
								tx_len,
								wx->task_id);

		wx->auth_offset += tx_len;
	} else {
		print(LOG_DBG, MODULE "AUTH: send auth req OK\n");
		osal_mem_free(wx->auth_data);
		wx->auth_data = NULL;
		wx->auth_len = 0;
		wx->auth_offset = 0;

//		wx->state = WECHAT_WAIT_AUTH_RESPONSE;
	}
}

static void __wechat_send_init(void)
{
	struct weixin *wx = get_wx();
	uint8 tx_len;

	if (wx->init_offset < wx->init_len) {
		tx_len = (wx->init_len - wx->init_offset < BLE_TX_MAX_LEN) ? wx->init_len - wx->init_offset : BLE_TX_MAX_LEN;

		print(LOG_DBG, MODULE "INIT: send init req: offset %d, len %d\n", wx->auth_offset, tx_len);
		ther_wechat_send_indicate(ble_get_gap_handle(),
								WECHAT_INDICATE_VALUE_POS,
								wx->init_data + wx->init_offset,
								tx_len,
								wx->task_id);

		wx->init_offset += tx_len;
	} else {
		print(LOG_DBG, MODULE "INIT: send init req OK\n");
		osal_mem_free(wx->init_data);
		wx->init_data = NULL;
		wx->init_len = 0;
		wx->init_offset = 0;

//		wx->state = WECHAT_WAIT_AUTH_RESPONSE;
	}
}

void ther_wechat_handle_indicate_confirm(uint8 *data)
{
	struct weixin *wx = get_wx();

	if (wx->state == WECHAT_SENDING_AUTH_REQUEST)
		__wechat_send_auth();

	if (wx->state == WECHAT_SENDING_INIT_REQUEST)
		__wechat_send_init();
}

void ther_wechat_send_auth_req(void)
{
	struct weixin *wx = get_wx();
	uint8 mac[B_ADDR_LEN];
	uint8 header_len = sizeof(struct wx_header);
	struct wx_header *header;
	uint16 len;
	AuthRequest req = {
		.base_request = {NULL},
		.has_md5_device_type_and_device_id = FALSE,
		.md5_device_type_and_device_id = {NULL, 0},
		.proto_version = PROTO_VERSION,
		.auth_proto = AUTH_PROTO,
		.auth_method = (EmAuthMethod)AUTH_METHOD,
		.has_aes_sign = FALSE,
		.aes_sign = {NULL, 0},
		.has_mac_address = TRUE,
		.mac_address = {mac, B_ADDR_LEN},
		.has_time_zone = FALSE,
		.time_zone = {NULL, 0},
		.has_language = FALSE,
		.language = {NULL, 0},
		.has_device_name = TRUE,
		.device_name = {DEVICE_ID, sizeof(DEVICE_ID)}
	};

	if (wx->auth_data)
		osal_mem_free(wx->auth_data);
	wx->auth_data = NULL;
	wx->auth_len = 0;
	wx->auth_offset = 0;

	ble_get_mac(mac);

	len = header_len + epb_auth_request_pack_size(&req);
	wx->auth_data = osal_mem_alloc(len);
	if (!wx->auth_data) {
		print(LOG_DBG, MODULE "AUTH: fail to alloc auth req buffer(len %d)\n", len);
		return;
	}

	/* fill body */
	if(epb_pack_auth_request(&req, wx->auth_data + header_len, len - header_len) < 0) {
		osal_mem_free(wx->auth_data);
		wx->auth_data = NULL;
		return;
	}

	/* fill header */
	header = (struct wx_header *)wx->auth_data;
	header->bMagicNumber = WX_HEADER_MAGIC;
	header->bVer = 1;
	header->nLength = htons(len);
	header->nCmdId = htons(ECI_req_auth);
	header->nSeq = htons(++wx->seq);

	print(LOG_DBG, MODULE "AUTH: start sending auth req, len = %d\n", len);
	wx->state = WECHAT_SENDING_AUTH_REQUEST;
	wx->auth_len = len;
	wx->auth_offset = 0;
	__wechat_send_auth();
}


void ther_wechat_send_init_req(void)
{
	struct weixin *wx = get_wx();
	uint8 header_len = sizeof(struct wx_header);
	struct wx_header *header;
	uint16 len;
	InitRequest req = {
		.base_request = {NULL},

		.has_resp_field_filter = FALSE,
		.resp_field_filter = {NULL, 0},

		.has_challenge = TRUE,
		.challenge = {challeange, CHALLENAGE_LENGTH},
	};

	if (wx->init_data)
		osal_mem_free(wx->init_data);
	wx->init_data = NULL;
	wx->init_len = 0;
	wx->init_offset = 0;

	len = header_len + epb_init_request_pack_size(&req);
	wx->init_data = osal_mem_alloc(len);
	if (!wx->init_data) {
		print(LOG_DBG, MODULE "INIT: fail to alloc init req buffer(len %d)\n", len);
		return;
	}

	/* fill body */
	if(epb_pack_init_request(&req, wx->init_data + header_len, len - header_len) < 0) {
		osal_mem_free(wx->init_data);
		wx->init_data = NULL;
		return;
	}

	/* fill header */
	header = (struct wx_header *)wx->init_data;
	header->bMagicNumber = WX_HEADER_MAGIC;
	header->bVer = 1;
	header->nLength = htons(len);
	header->nCmdId = htons(ECI_req_init);
	header->nSeq = htons(++wx->seq);

	print(LOG_DBG, MODULE "INIT: start sending init req, len = %d\n", len);
	wx->state = WECHAT_SENDING_INIT_REQUEST;
	wx->init_len = len;
	wx->init_offset = 0;
	__wechat_send_init();
}

static uint16 __ther_wechat_dispatch_cmd(uint8 *data, uint16 len)
{
	struct weixin *wx = get_wx();
	struct wx_header *header = (struct wx_header *)data;
	uint8 header_len = sizeof(struct wx_header);

	{
		uint8 i;

		print(LOG_DBG, "------\n");
		print(LOG_DBG, "packet len %d, details:\n", len);
		for (i = 0; i < len; i++) {
			if (i > 0 && i % 10 == 0)
				print(LOG_DBG, "\n");

			print(LOG_DBG, "%02X ", data[i]);
		}
		print(LOG_DBG, "\n------\n");
	}

	switch (ntohs(header->nCmdId)) {
	case ECI_none:
		break;

	case ECI_resp_auth:
		AuthResponse *auth_resp;

		print(LOG_DBG, MODULE "AUTH: receive auth response\n");
		auth_resp = epb_unpack_auth_response(data + header_len, len - header_len);
		if (!auth_resp) {
			print(LOG_DBG, MODULE "AUTH: fail to unpack auth response\n");
			return errorCodeUnpackAuthResp;
		}

		if (auth_resp->base_response) {

		}

		epb_unpack_auth_response_free(auth_resp);

		if (wx->state != WECHAT_WAIT_AUTH_RESPONSE) {
			print(LOG_DBG, MODULE "AUTH: why get auth response in state %d\n", wx->state);
		}

		ther_wechat_send_init_req();
		break;

	case ECI_resp_init:
		InitResponse *init_resp;

		print(LOG_DBG, MODULE "INIT: receive init response\n");
		init_resp = epb_unpack_init_response(data + header_len, len - header_len);
		if(!init_resp) {
			print(LOG_DBG, MODULE "INIT: fail to unpack init response\n");
			return errorCodeUnpackInitResp;
		}

		if(init_resp->base_response) {

		}

		epb_unpack_init_response_free(init_resp);

		break;

	default:
		print(LOG_DBG, MODULE "unknown cmd %d\n", ntohs(header->nCmdId));
		break;
	}

	return 0;
}

void ther_wechat_write(uint8 *data, uint8 *len)
{
	struct weixin *wx = get_wx();

	if (!wx->recv_data) {
		struct wx_header *header = (struct wx_header *)data;

		wx->recv_len = ntohs(header->nLength);
		wx->recv_offset = 0;
		wx->recv_data = osal_mem_alloc(wx->recv_len);
		if (!wx->recv_data) {
			print(LOG_DBG, MODULE "fail to allocate recv buffer(len %d)\n", wx->recv_len);
			return;
		}
		print(LOG_DBG, MODULE "get cmd header\n");
		print(LOG_DBG, MODULE "    magic %X\n", header->bMagicNumber);
		print(LOG_DBG, MODULE "    cmd   %d\n", ntohs(header->nCmdId));
		print(LOG_DBG, MODULE "    len   %d\n", ntohs(header->nLength));
		print(LOG_DBG, MODULE "    seq   %d\n", ntohs(header->nSeq));
	}

	if (*len > wx->recv_len - wx->recv_offset) {
		print(LOG_DBG, MODULE "ther_wechat_write():　overflow？？  (%d > %d)\n", *len, wx->recv_len - wx->recv_offset);
		goto free_recv_data;
	}

	memcpy(wx->recv_data + wx->recv_offset, data, *len);
	wx->recv_offset += *len;

	if (wx->recv_offset >= wx->recv_len) {
		__ther_wechat_dispatch_cmd(wx->recv_data, wx->recv_len);
		goto free_recv_data;
	}

	return;

free_recv_data:
	osal_mem_free(wx->recv_data);
	wx->recv_data = NULL;
	wx->recv_len = 0;
	wx->recv_offset = 0;

	return;
}

void ther_wechat_init(uint8 task_id)
{
	wx = osal_mem_alloc(sizeof(struct weixin));
	if (!wx) {
		print(LOG_DBG, MODULE "fail to allocate weixin struct\n");
		return;
	}

	memset(wx, 0, sizeof(struct weixin));

	wx->task_id = task_id;
	wx->state = WECHAT_WAIT_INDICATE_ENABLE;
}

void ther_wechat_exit(void)
{
	osal_mem_free(wx);
	wx = NULL;
}
