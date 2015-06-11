
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
#include "ther_profile.h"
#include "devinfoservice.h"
#include "thermometer.h"
#include "timeapp.h"
#include "OSAL_Clock.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_temp.h"
#include "ther_comm.h"

#define MODULE "[THER COMM] "

#define THER_INDICATE_FLAG (THERMOMETER_FLAGS_CELCIUS | THERMOMETER_FLAGS_TIMESTAMP | THERMOMETER_FLAGS_TYPE)
#define THER_NOTIFY_FLAG (THERMOMETER_FLAGS_CELCIUS | THERMOMETER_FLAGS_TIMESTAMP | THERMOMETER_FLAGS_TYPE)

static unsigned char encap_temp_data(unsigned char flag, unsigned short temp, unsigned char *buf)
{
	unsigned char *start_start = buf;
	unsigned long format_temp;

	/* a). flag */
	*buf++ = flag;

	if (flag & THERMOMETER_FLAGS_FARENHEIT)
		temp  = (temp * 9 / 5) + 320;

	format_temp = 0xFF000000 | temp;

	/* b). temp */
	osal_buffer_uint32(buf, format_temp);
	buf += 4;

    /* c). timestamp */
    if (flag & THERMOMETER_FLAGS_TIMESTAMP) {
      UTCTimeStruct time;

      // Get time structure from OSAL
      osal_ConvertUTCTime( &time, osal_getClock() );

      *buf++ = (time.year & 0x00FF);
      *buf++ = (time.year & 0xFF00)>>8;
      //*p++ = time.year;
      //*p++;

      *buf++ = time.month;
      *buf++ = time.day;
      *buf++ = time.hour;
      *buf++ = time.minutes;
      *buf++ = time.seconds;
    }

    /* d). type */
	if(flag & THERMOMETER_FLAGS_TYPE)
	{
		uint8 type;
		Thermometer_GetParameter( THERMOMETER_TYPE, &type );
		*buf++ =  type;
	}

	return buf - start_start;
}

void ther_send_temp_notify(uint16 connect_handle, unsigned short temp)
{
	attHandleValueNoti_t notify;

	notify.len = encap_temp_data(THER_NOTIFY_FLAG, temp, notify.value);
//	notify.handle = THERMOMETER_IMEAS_VALUE_POS;

	print(LOG_DBG, MODULE "send notification(temp %d)\r\n", temp);

	Thermometer_IMeasNotify( connect_handle, &notify);

	return;
}

void ther_send_temp_indicate(uint16 connect_handle, unsigned char task_id, unsigned short temp)
{
	attHandleValueInd_t indicate;

	indicate.len = encap_temp_data(THER_INDICATE_FLAG, temp, indicate.value);
	indicate.handle = THERMOMETER_TEMP_VALUE_POS;

	print(LOG_DBG, MODULE "send indication(temp %d)\r\n", temp);

	Thermometer_TempIndicate( connect_handle, &indicate, task_id);

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
		timeAppIndGattMsg( msg );
	}
	else if ( msg->method == ATT_READ_RSP ||
			msg->method == ATT_WRITE_RSP )
	{
	}
	else
	{
	}
}
