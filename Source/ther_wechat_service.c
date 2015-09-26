/*
 * THER BLE PRIVATE SERVICE
 *
 * Copyright (c) 2015 by Leo Liu <59089403@qq.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 *
 * 2015/06/01 - Init version
 *              by Leo Liu <59089403@qq.com>
 *
 */


#include <string.h>

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "linkdb.h"
#include "att.h"
#include "gatt.h"
#include "gatt_uuid.h"
#include "gattservapp.h"
#include "gapbondmgr.h"

#include "ther_uart.h"
#include "config.h"
#include "thermometer.h"
#include "ther_wechat_service.h"

#define MODULE "[THER WX] "

#define TIME_LEN 7
struct ther_wechat_info {
	void (*handle_wechat_event)(uint8 event, uint8 *data, uint8 *len);

	uint8 dummy_val;
/*	uint8 warning_enabled;
	uint8 clear_warning;
	unsigned short high_temp;
	unsigned char time[TIME_LEN];*/
};

struct ther_wechat_info wechat_info;

// Simple Profile Characteristic 4 Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t wechatProfileChar4Config[GATT_MAX_NUM_CONN];

#define THER_WECHAT_WRITE_UUID			0xFEC7
#define THER_WECHAT_INDICATE_UUID		0xFEC8
#define THER_WECHAT_READ_UUID			0xFEC9

static const uint8 ther_wechat_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_WECHAT_UUID), HI_UINT16(THER_WECHAT_UUID)
};

static const gattAttrType_t ther_wechat = {ATT_BT_UUID_SIZE, ther_wechat_uuid};

/*
 * wechat_indicate
 */
const uint8 ther_wechat_indicate_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_WECHAT_INDICATE_UUID), HI_UINT16(THER_WECHAT_INDICATE_UUID)
};
static uint8 ther_wechat_indicate_props = GATT_PROP_INDICATE;
static gattCharCfg_t wechatIndicateConfig[GATT_MAX_NUM_CONN];
//static uint8 ther_wechat_indicate_user_desp[] = "wechat indicate";


/*
 * wechat_write
 */
const uint8 ther_wechat_write_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_WECHAT_WRITE_UUID), HI_UINT16(THER_WECHAT_WRITE_UUID)
};
static uint8 ther_wechat_write_props = GATT_PROP_WRITE;
static uint8 ther_wechat_write_user_desp[] = "wechat write";

/*
 * wechat_read
 */
const uint8 ther_wechat_read_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_WECHAT_READ_UUID), HI_UINT16(THER_WECHAT_READ_UUID)
};
static uint8 ther_wechat_read_props = GATT_PROP_READ;
static uint8 ther_wechat_read_user_desp[] = "wechat read";

static gattAttribute_t ther_wechat_attr_table[] = {
	{
		{ATT_BT_UUID_SIZE, primaryServiceUUID},	/* type */
		GATT_PERMIT_READ,						/* permissions */
		0,										/* handle */
		(uint8 *)&ther_wechat					/* pValue */
	},

	/*
	 * Declaration: wechat indicate
	 */

		// properties of <wechat indicate>
		{
			{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2804 */
			GATT_PERMIT_READ,
			0,
			&ther_wechat_indicate_props
		},

		// value of <wechat indicate>
		{
			{ ATT_BT_UUID_SIZE, ther_wechat_indicate_uuid},	/* 0xFEC8 */
			0,
			0,
//				(unsigned char *)&wechat_info.time
			&wechat_info.dummy_val
		},

	    // Characteristic Configuration of <wechat indicate>
	    {
	      { ATT_BT_UUID_SIZE, clientCharCfgUUID },
	      GATT_PERMIT_READ | GATT_PERMIT_WRITE,
	      0,
	      (uint8 *)&wechatIndicateConfig
	    },

		/*
		 * Declaration: wechat write
		 */

			// properties of <wechat write>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2803 */
				GATT_PERMIT_READ,
				0,
				&ther_wechat_write_props
			},

			// value of <wechat write>
			{
				{ ATT_BT_UUID_SIZE, ther_wechat_write_uuid},	/* 0xFEC7 */
				GATT_PERMIT_WRITE,
				0,
//				&wechat_info.clear_warning
				&wechat_info.dummy_val
			},

			// user Description of <wechat write>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_wechat_write_user_desp
			},

		/*
		 * Declaration: wechat read
		 */

			// properties of <wechat read>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2803 */
				GATT_PERMIT_READ,
				0,
				&ther_wechat_read_props
			},

			// value of <wechat read>
			{
				{ ATT_BT_UUID_SIZE, ther_wechat_read_uuid},	/* 0xFEC9 */
				GATT_PERMIT_READ,
				0,
//				(unsigned char *)&wechat_info.high_temp
				&wechat_info.dummy_val
			},

			// user Description of <wechat read>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_wechat_read_user_desp
			},

};


/*********************************************************************
 * @fn          simpleProfile_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 *
 * @return      Success or Failure
 */
static uint8 ther_wechat_read_attr(uint16 connHandle, gattAttribute_t *pAttr,
                            uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen)
{
	struct ther_wechat_info *ps = &wechat_info;
	bStatus_t status = SUCCESS;
	uint16 uuid;

	/* 16-bit UUID */
	uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);

	print(LOG_DBG, MODULE "read connHandle 0x%x, uuid 0x%x, offset %d, len %d, *pLen = %d\n",
			connHandle, uuid, offset, maxLen, *pLen); // maxLen is alwarys 22, *pLen is 1 or 14 or 9 ??

	/* If attribute permissions require authorization to read, return error */
	if (gattPermitAuthorRead(pAttr->permissions)) {
		print(LOG_DBG, MODULE "err: Insufficient authorization\n");
		return ( ATT_ERR_INSUFFICIENT_AUTHOR );
	}

	/* Make sure it's not a blob operation (no attributes in the profile are long) */
	if (offset > 0) {
		print(LOG_DBG, MODULE "err: a blob operation\n");
		return ATT_ERR_ATTR_NOT_LONG;
	}

	if (pAttr->type.len != ATT_BT_UUID_SIZE) {
		print(LOG_DBG, MODULE "err: 128-bit UUID\n");

		// 128-bit UUID
		*pLen = 0;
		return ATT_ERR_INVALID_HANDLE;
	}

	switch (uuid) {
	case THER_WECHAT_READ_UUID:
/*		memcpy(pValue, pAttr->pValue, 2);
		*pLen = 2;*/
		ps->handle_wechat_event(THER_WECHAT_DATE_READ, pValue, pLen);

		break;

	default:
		print(LOG_DBG, MODULE "do not support uuid %x read\n", uuid);

		*pLen = 0;
		status = ATT_ERR_ATTR_NOT_FOUND;
		break;
	}

	return status;
}

/*********************************************************************
 * @fn      simpleProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 *
 * @return  Success or Failure
 */
static bStatus_t ther_wechat_write_attr(uint16 connHandle, gattAttribute_t *pAttr,
                                 uint8 *pValue, uint8 len, uint16 offset)
{
	struct ther_wechat_info *ws = &wechat_info;
	bStatus_t status = SUCCESS;
	uint16 uuid;

	/* 16-bit UUID */
	uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1]);

	print(LOG_DBG, MODULE "write connHandle 0x%x, uuid 0x%x, offset %d, len %d\n",
			connHandle, uuid, offset, len);

	/* If attribute permissions require authorization to write, return error */
	if (gattPermitAuthorWrite(pAttr->permissions)) {
		print(LOG_DBG, MODULE "err: Insufficient authorization\n");
		return ( ATT_ERR_INSUFFICIENT_AUTHOR );
	}

	if (pAttr->type.len != ATT_BT_UUID_SIZE) {
		print(LOG_DBG, MODULE "err: 128-bit UUID\n");

		// 128-bit UUID
		return ATT_ERR_INVALID_HANDLE;
	}

	switch (uuid) {
	case GATT_CLIENT_CHAR_CFG_UUID:
		// Validate/Write wechat indicate setting
		if ( pAttr->handle == ther_wechat_attr_table[WECHAT_INDICATE_CHAT_CONFIG_POS].handle) {
			status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len,
			                                                 offset, GATT_CLIENT_CFG_INDICATE);

	        if ( status == SUCCESS )
	        {
	          uint16 value = BUILD_UINT16(pValue[0], pValue[1]);

	          ws->handle_wechat_event(THER_WECHAT_SET_INDICATE, (uint8 *)&value, &len);
	        }
		} else {
			status = ATT_ERR_INVALID_VALUE_SIZE;
		}
		break;

	case THER_WECHAT_WRITE_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else {
//			*pAttr->pValue = *pValue;
			ws->handle_wechat_event(THER_WECHAT_DATA_WRITE, pValue, &len);
		}
		break;

	default:
		status = ATT_ERR_ATTR_NOT_FOUND;
		break;
	}

	return status;
}

static const gattServiceCBs_t ther_wechat_rw_hooks =
{
	ther_wechat_read_attr,  // Read callback function pointer
	ther_wechat_write_attr, // Write callback function pointer
	NULL                       // Authorization callback function pointer
};

static void ther_wechat_handle_connect_status(uint16 connHandle, uint8 changeType)
{
	// Make sure this is not loopback connection
	if ( connHandle != LOOPBACK_CONNHANDLE ) {
		// Reset Client Char Config if connection has dropped
		if ((changeType == LINKDB_STATUS_UPDATE_REMOVED ) ||
			((changeType == LINKDB_STATUS_UPDATE_STATEFLAGS) && !linkDB_Up(connHandle))) {
			GATTServApp_InitCharCfg( connHandle, wechatProfileChar4Config );
			GATTServApp_InitCharCfg( connHandle, wechatIndicateConfig );
		}
	}
}

bStatus_t ther_wechat_send_indicate(uint16 connHandle, uint16 handle, uint8 *data, uint8 len, uint8 taskId )
{
	attHandleValueInd_t notify;
	uint16 value = GATTServApp_ReadCharCfg(connHandle, wechatIndicateConfig);

	memcpy(notify.value, data, len);
	notify.len = len;

	// If indications enabled
	if (value & GATT_CLIENT_CFG_INDICATE)
	{
		// Set the handle (uses stored relative handle to lookup actual handle)
		notify.handle = ther_wechat_attr_table[handle].handle;

		// Send the Indication
		return GATT_Indication(connHandle, &notify, FALSE, taskId);
	}

  return bleIncorrectMode;
}

bStatus_t ther_add_wechat_service(uint32 services, void (*handle_wechat_event)(uint8 event, uint8 *data, uint8 *len))
{
	struct ther_wechat_info *ws = &wechat_info;
	uint8 status = SUCCESS;

	ws->handle_wechat_event = handle_wechat_event;

	GATTServApp_InitCharCfg(INVALID_CONNHANDLE, wechatProfileChar4Config);
	GATTServApp_InitCharCfg(INVALID_CONNHANDLE, wechatIndicateConfig);

	// Register with Link DB to receive link status change callback
	VOID linkDB_Register(ther_wechat_handle_connect_status);

	if ( services & THER_WECHAT_SERVICE) {
		// Register GATT attribute list and CBs with GATT Server App
		status = GATTServApp_RegisterService( ther_wechat_attr_table,
								  GATT_NUM_ATTRS(ther_wechat_attr_table),
								  &ther_wechat_rw_hooks);
	}

	return status;
}
