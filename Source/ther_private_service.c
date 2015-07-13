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

#include "thermometer.h"
#include "ther_private_service.h"

#define MODULE "[THER PS] "

#define TIME_LEN 7
struct ther_ps_info {
	void (*handle_ps_event)(uint8 event, uint8 *data, uint8 *len);

	uint8 dummy_val;
/*	uint8 warning_enabled;
	uint8 clear_warning;
	unsigned short high_temp;
	unsigned char time[TIME_LEN];*/
};

struct ther_ps_info ps_info;

// Simple Profile Characteristic 4 Configuration Each client has its own
// instantiation of the Client Characteristic Configuration. Reads of the
// Client Characteristic Configuration only shows the configuration for
// that client and writes only affect the configuration of that client.
static gattCharCfg_t simpleProfileChar4Config[GATT_MAX_NUM_CONN];

// Simple Profile Service UUID
#define THER_PS_UUID 0xFFF0

#define THER_PS_WARNING_ENABLED_UUID	0xFFF1
#define THER_PS_CLEAR_WARNING_UUID		0xFFF2
#define THER_PS_HIGH_TEMP_UUID			0xFFF3
#define THER_PS_TIME_UUID				0xFFF4
#define THER_PS_DEBUG_UUID				0xFFF5



static const uint8 ther_ps_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_UUID), HI_UINT16(THER_PS_UUID)
};

static const gattAttrType_t ther_ps = {ATT_BT_UUID_SIZE, ther_ps_uuid};

/*
 * warning enabled
 */
const uint8 ther_ps_warning_enabled_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_WARNING_ENABLED_UUID), HI_UINT16(THER_PS_WARNING_ENABLED_UUID)
};
static uint8 ther_ps_warning_enabled_props = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8 ther_ps_warning_enabled_user_desp[] = "enable warning";

/*
 * clear warning
 */
const uint8 ther_ps_clear_warning_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_CLEAR_WARNING_UUID), HI_UINT16(THER_PS_CLEAR_WARNING_UUID)
};
static uint8 ther_ps_clear_warning_props = GATT_PROP_WRITE;
static uint8 ther_ps_clear_warning_user_desp[] = "clear warning";

/*
 * high temp
 */
const uint8 ther_ps_high_temp_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_HIGH_TEMP_UUID), HI_UINT16(THER_PS_HIGH_TEMP_UUID)
};
static uint8 ther_ps_high_temp_props = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8 ther_ps_high_temp_user_desp[] = "high temp";

/*
 * time
 */
const uint8 ther_ps_time_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_TIME_UUID), HI_UINT16(THER_PS_TIME_UUID)
};
static uint8 ther_ps_time_props = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8 ther_ps_time_user_desp[] = "time";

/*
 * debug
 */
const uint8 ther_ps_debug_uuid[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(THER_PS_DEBUG_UUID), HI_UINT16(THER_PS_DEBUG_UUID)
};
static uint8 ther_ps_debug_props = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8 ther_ps_debug_user_desp[] = "debug";

static gattAttribute_t ther_ps_attr_table[] = {
	{
		{ATT_BT_UUID_SIZE, primaryServiceUUID},	/* type */
		GATT_PERMIT_READ,						/* permissions */
		0,										/* handle */
		(uint8 *)&ther_ps						/* pValue */
	},

		/*
		 * Declaration: warning enabled
		 */

			// properties of <warning enabled>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2803 */
				GATT_PERMIT_READ,
				0,
				&ther_ps_warning_enabled_props
			},

			// value of <warning enabled>
			{
				{ ATT_BT_UUID_SIZE, ther_ps_warning_enabled_uuid},	/* 0xFFF1 */
				GATT_PERMIT_READ | GATT_PERMIT_WRITE,
				0,
//				&ps_info.warning_enabled
				&ps_info.dummy_val
			},

			// user Description of <warning enabled>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_ps_warning_enabled_user_desp
			},


		/*
		 * Declaration: clear warning
		 */

			// properties of <clear warning>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2803 */
				GATT_PERMIT_READ,
				0,
				&ther_ps_clear_warning_props
			},

			// value of <clear warning>
			{
				{ ATT_BT_UUID_SIZE, ther_ps_clear_warning_uuid},	/* 0xFFF2 */
				GATT_PERMIT_WRITE,
				0,
//				&ps_info.clear_warning
				&ps_info.dummy_val
			},

			// user Description of <clear warning>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_ps_clear_warning_user_desp
			},

		/*
		 * Declaration: high temp
		 */

			// properties of <high temp>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2803 */
				GATT_PERMIT_READ,
				0,
				&ther_ps_high_temp_props
			},

			// value of <high temp>
			{
				{ ATT_BT_UUID_SIZE, ther_ps_high_temp_uuid},	/* 0xFFF3 */
				GATT_PERMIT_READ | GATT_PERMIT_WRITE,
				0,
//				(unsigned char *)&ps_info.high_temp
				&ps_info.dummy_val
			},

			// user Description of <high temp>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_ps_high_temp_user_desp
			},

		/*
		 * Declaration: time
		 */

			// properties of <time>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2804 */
				GATT_PERMIT_READ,
				0,
				&ther_ps_time_props
			},

			// value of <time>
			{
				{ ATT_BT_UUID_SIZE, ther_ps_time_uuid},	/* 0xFFF4 */
				GATT_PERMIT_READ | GATT_PERMIT_WRITE,
				0,
//					(unsigned char *)&ps_info.time
				&ps_info.dummy_val
			},

			// user Description of <time>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_ps_time_user_desp
			},

		/*
		 * Declaration: debug
		 */

			// properties of <debug>
			{
				{ ATT_BT_UUID_SIZE, characterUUID},	/* 0x2804 */
				GATT_PERMIT_READ,
				0,
				&ther_ps_debug_props
			},

			// value of <debug>
			{
				{ ATT_BT_UUID_SIZE, ther_ps_debug_uuid},	/* 0xFFF5 */
				GATT_PERMIT_READ | GATT_PERMIT_WRITE,
				0,
//					(unsigned char *)&ps_info.time
				&ps_info.dummy_val
			},

			// user Description of <debug>
			{
				{ ATT_BT_UUID_SIZE, charUserDescUUID },
				GATT_PERMIT_READ,
				0,
				ther_ps_debug_user_desp
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
static uint8 ther_ps_read_attr(uint16 connHandle, gattAttribute_t *pAttr,
                            uint8 *pValue, uint8 *pLen, uint16 offset, uint8 maxLen)
{
	struct ther_ps_info *ps = &ps_info;
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
	case THER_PS_WARNING_ENABLED_UUID:
/*		pValue[0] = *pAttr->pValue;
		*pLen = 1;*/
		ps->handle_ps_event(THER_PS_GET_WARNING_ENABLED, pValue, pLen);

		break;

	case THER_PS_HIGH_TEMP_UUID:
/*		memcpy(pValue, pAttr->pValue, 2);
		*pLen = 2;*/
		ps->handle_ps_event(THER_PS_GET_HIGH_WARN_TEMP, pValue, pLen);

		break;

	case THER_PS_TIME_UUID:
/*		memcpy(pValue, pAttr->pValue, TIME_LEN);
		*pLen = TIME_LEN;*/
		ps->handle_ps_event(THER_PS_GET_TIME, pValue, pLen);

		break;

	case THER_PS_DEBUG_UUID:
		ps->handle_ps_event(THER_PS_GET_DEBUG, pValue, pLen);
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
static bStatus_t ther_ps_write_attr(uint16 connHandle, gattAttribute_t *pAttr,
                                 uint8 *pValue, uint8 len, uint16 offset)
{
	struct ther_ps_info *ps = &ps_info;
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
	case THER_PS_WARNING_ENABLED_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else if (len != 1)
			status = ATT_ERR_INVALID_VALUE_SIZE;
		else {
			//	*pAttr->pValue = *pValue;
			ps->handle_ps_event(THER_PS_SET_WARNING_ENABLED, pValue, &len);
		}
		break;

	case THER_PS_CLEAR_WARNING_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else if (len != 1)
			status = ATT_ERR_INVALID_VALUE_SIZE;
		else {
//			*pAttr->pValue = *pValue;
			ps->handle_ps_event(THER_PS_CLEAR_WARNING, pValue, &len);
		}
		break;

	case THER_PS_HIGH_TEMP_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else if (len != 2)
			status = ATT_ERR_INVALID_VALUE_SIZE;
		else {
/*			memcpy(pAttr->pValue, pValue, len);*/
			ps->handle_ps_event(THER_PS_SET_HIGH_WARN_TEMP, pValue, &len);
		}
		break;

	case THER_PS_TIME_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else if (len != TIME_LEN)
			status = ATT_ERR_INVALID_VALUE_SIZE;
		else {
/*			memcpy(pAttr->pValue, pValue, len);*/
			ps->handle_ps_event(THER_PS_SET_TIME, pValue, &len);
		}
		break;

	case THER_PS_DEBUG_UUID:
		/*
		 * Validate the value. Make sure it's not a blob oper
		 */
		if (offset !=0)
			status = ATT_ERR_ATTR_NOT_LONG;
		else if (len != TIME_LEN)
			status = ATT_ERR_INVALID_VALUE_SIZE;
		else {
			ps->handle_ps_event(THER_PS_SET_DEBUG, pValue, &len);
		}
		break;

	default:
		status = ATT_ERR_ATTR_NOT_FOUND;
		break;
	}

	return status;
}

static const gattServiceCBs_t ther_ps_rw_hooks =
{
	ther_ps_read_attr,  // Read callback function pointer
	ther_ps_write_attr, // Write callback function pointer
	NULL                       // Authorization callback function pointer
};

static void ther_ps_handle_connect_status(uint16 connHandle, uint8 changeType)
{
	// Make sure this is not loopback connection
	if ( connHandle != LOOPBACK_CONNHANDLE ) {
		// Reset Client Char Config if connection has dropped
		if ((changeType == LINKDB_STATUS_UPDATE_REMOVED ) ||
			((changeType == LINKDB_STATUS_UPDATE_STATEFLAGS) && !linkDB_Up(connHandle))) {
			GATTServApp_InitCharCfg( connHandle, simpleProfileChar4Config );
		}
	}
}

#if 0
bStatus_t ther_ps_set_param(uint8 param, unsigned char *value)
{
	struct ther_ps_info *ps = &ps_info;
	bStatus_t ret = SUCCESS;

	switch (param) {
	case THER_PS_SET_WARNING_ENABLED:
		ps->warning_enabled = *value;

		break;

	case THER_PS_CLEAR_WARNING:
		ps->clear_warning = *value;
		break;

	case THER_PS_SET_HIGH_WARN_TEMP:
		ps->high_temp = *(unsigned short *)value;
		break;

	case THER_PS_SET_TIME:
		memcpy(ps->time, value, TIME_LEN);
		break;

	default:
		ret = INVALIDPARAMETER;
		break;
	}

	return ret;
}

bStatus_t ther_ps_get_param( uint8 param, unsigned char *value)
{
	struct ther_ps_info *ps = &ps_info;

	bStatus_t ret = SUCCESS;
	switch ( param ) {
	case THER_PS_SET_WARNING_ENABLED:
		*value = ps->warning_enabled;

		break;

	case THER_PS_CLEAR_WARNING:
		*value = ps->clear_warning;
		break;

	case THER_PS_SET_HIGH_WARN_TEMP:
		*(unsigned short *)value = ps->high_temp;
		break;

	case THER_PS_SET_TIME:
		memcpy(value, ps->time, TIME_LEN);
		break;

	default:
		ret = INVALIDPARAMETER;
		break;
	}

  return ret;
}
#endif

bStatus_t ther_add_private_service(uint32 services, void (*handle_ps_event)(uint8 event, uint8 *data, uint8 *len))
{
	struct ther_ps_info *ps = &ps_info;
	uint8 status = SUCCESS;

	ps->handle_ps_event = handle_ps_event;

	GATTServApp_InitCharCfg(INVALID_CONNHANDLE, simpleProfileChar4Config);

	// Register with Link DB to receive link status change callback
	VOID linkDB_Register(ther_ps_handle_connect_status);

	if ( services & THER_PRIVATE_SERVICE) {
		// Register GATT attribute list and CBs with GATT Server App
		status = GATTServApp_RegisterService( ther_ps_attr_table,
								  GATT_NUM_ATTRS(ther_ps_attr_table),
								  &ther_ps_rw_hooks);
	}

	return status;
}
