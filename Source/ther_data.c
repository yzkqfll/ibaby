/*
 * THER DATA
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
#include "OSAL_PwrMgr.h"
#include "OnBoard.h"
#include "hal_adc.h"
#include "hal_led.h"
#include "hal_lcd.h"
#include "hal_key.h"
#include "gatt.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "gatt_profile_uuid.h"
#include "linkdb.h"
#include "peripheral.h"
#include "gapbondmgr.h"
#include "ther_service.h"
#include "thermometer.h"

#include "ther_uart.h"

#include "ther_temp.h"
#include "ther_data.h"
#include "ther_ble.h"
#include "ther_storage.h"

#define MODULE "[DATA   ] "

#define THER_INDICATE_FLAG (THERMOMETER_FLAGS_CELCIUS | THERMOMETER_FLAGS_TIMESTAMP | THERMOMETER_FLAGS_TYPE)
#define THER_NOTIFY_FLAG (THERMOMETER_FLAGS_CELCIUS | THERMOMETER_FLAGS_TIMESTAMP | THERMOMETER_FLAGS_TYPE)

static const char *ther_type[] = {
	"",
	"arm",
	"body",
	"ear",
	"finger",
	"gastro",
	"mouth",
	"rectum",
	"toe",
	"tympnum"
};

static void encap_temp_data(struct temp_data *td, unsigned char flag, unsigned short temp)
{
	uint32 format_temp;

	/* flag */
	td->flag = flag;

	/* temp */
	if (flag & THERMOMETER_FLAGS_FARENHEIT)
		temp  = (temp * 9 / 5) + 320;

	format_temp = 0xFF000000 | temp;

	td->temp = format_temp;

	/* timestamp */
	if (flag & THERMOMETER_FLAGS_TIMESTAMP) {
		UTCTimeStruct time;

		// Get time structure from OSAL
		osal_ConvertUTCTime(&time, osal_getClock());

		td->year = time.year;
		td->month = time.month;
		td->day = time.day;
		td->hour = time.hour;
		td->minutes = time.minutes;
		td->seconds = time.seconds;
	}

    /* type */
	if(flag & THERMOMETER_FLAGS_TYPE)
	{
		uint8 type;

		Thermometer_GetParameter( THERMOMETER_TYPE, &type );
		td->type = type;
	}
}

void ther_send_temp_notify(unsigned short temp)
{
	attHandleValueNoti_t notify;
	struct temp_data td;

	encap_temp_data(&td, THER_NOTIFY_FLAG, temp);
	memcpy(notify.value, &td, sizeof(td));
	notify.len = sizeof(td);

//	notify.handle = THERMOMETER_IMEAS_VALUE_POS;

	print(LOG_DBG, MODULE "send notification(temp %d)\n", temp);

	Thermometer_IMeasNotify(ble_get_gap_handle(), &notify);

	return;
}

void ther_send_temp_indicate(unsigned char task_id, unsigned short temp)
{
	attHandleValueInd_t indicate;
	struct temp_data td;

	encap_temp_data(&td, THER_INDICATE_FLAG, temp);
	memcpy(indicate.value, &td, sizeof(td));
	indicate.len = sizeof(td);

	indicate.handle = THERMOMETER_TEMP_VALUE_POS;

	print(LOG_DBG, MODULE "send indication(temp %d)\n", temp);

	Thermometer_TempIndicate(ble_get_gap_handle(), &indicate, task_id);

	return;
}

void ther_save_temp_to_local(unsigned short temp)
{
	struct temp_data td;
    UTCTimeStruct time;

    // Get time structure from OSAL
    osal_ConvertUTCTime(&time, osal_getClock());
    if (0 && time.year < 2015) {
    	print(LOG_DBG, MODULE "time not calibrated, ignore temp save\n");
    	return;
    }

    encap_temp_data(&td, THER_INDICATE_FLAG, temp);
	storage_save_temp((uint8 *)&td, sizeof(td));
}

void ther_send_history_temp2(unsigned char task_id, uint8 *data, uint8 len)
{
	attHandleValueInd_t indicate;
	struct temp_data *td = (struct temp_data *)data;

	memcpy(indicate.value, data, len);
	indicate.len = len;
	indicate.handle = THERMOMETER_TEMP_VALUE_POS;

	print(LOG_DBG, MODULE "Upload: %d-%02d-%02d %02d:%02d:%02d, temp %ld, %s\n",
			td->year, td->month, td->day, td->hour, td->minutes, td->seconds,
			td->temp & 0xFFFFFF, ther_type[td->type]);

	Thermometer_TempIndicate(ble_get_gap_handle(), &indicate, task_id);

	return;
}

void ther_send_history_temp(unsigned char task_id, uint8 *data, uint8 len)
{
	attHandleValueNoti_t notify;
	struct temp_data *td = (struct temp_data *)data;

	memcpy(notify.value, data, len);
	notify.len = len;
//	indicate.handle = THERMOMETER_TEMP_VALUE_POS;  // notify do not need this

	print(LOG_DBG, MODULE "Upload: %d-%02d-%02d %02d:%02d:%02d, temp %ld, %s\n",
			td->year, td->month, td->day, td->hour, td->minutes, td->seconds,
			td->temp & 0xFFFFFF, ther_type[td->type]);

	Thermometer_IMeasNotify(ble_get_gap_handle(), &notify);

	return;
}


void ther_handle_gatt_msg(gattMsgEvent_t *msg)
{
	/*
	 * Indication Confirmation
	 */
	if(msg->method == ATT_HANDLE_VALUE_CFM)
	{
//		thermometerSendStoredMeas();
	}

	if (msg->method == ATT_HANDLE_VALUE_NOTI ||
			msg->method == ATT_HANDLE_VALUE_IND )
	{
//		timeAppIndGattMsg( msg );
	}
	else if ( msg->method == ATT_READ_RSP ||
			msg->method == ATT_WRITE_RSP )
	{
	}
	else
	{
	}
}
