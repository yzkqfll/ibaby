/*
 * THER BLE
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
#include "OSAL_Clock.h"

#include "ther_batt_service.h"
#include "ther_private_service.h"
#include "ther_devinfo_service.h"

#include "ther_uart.h"
#include "ther_ble.h"
#include <stdio.h>
#include "config.h"

#define MODULE "[BLE    ] "

#define BLE_DEVICE_NAME "NeoBay"

struct ble_info {
	uint8 task_id;

	void (*handle_ts_event)(unsigned char event);
	void (*handle_ps_event)(unsigned char event, unsigned char *data, unsigned char *len);

	uint16 gap_handle;
	gaprole_States_t gap_role_state;

	uint8 last_connected_addr[B_ADDR_LEN];
	bool connect_to_last_addr;


	unsigned char advertising_enable;

	/*
	 * By setting this to zero, the device will go into the waiting state after
	 * being discoverable for 30.72 second, and will not being advertising again
	 * until the enabler is set back to TRUE
	 */
	unsigned short advertising_off_time;

	/* Whether to enable automatic parameter update request when a connection is formed */
	unsigned char update_request_enable;

	unsigned short min_connection_interval; /* Minimum connection interval (units of 1.25ms) */
	unsigned short max_connection_interval; /* Maximum connection interval (units of 1.25ms) */
	unsigned short slave_latency;
	unsigned short connection_timeout; /* Supervision timeout value (units of 10ms) */
};

static struct ble_info ble_info;

static const char *gap_state_str[] = {
	"init",
	"started",
	"advertising",
	"waiting",
	"waiting_after_timeout",
	"connected",
	"connected_adv",
	"error"
};

// Use limited discoverable mode to advertise for 30.72s, and then stop, or
// use general discoverable mode to advertise indefinitely
//#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_LIMITED
#define BLE_DISCOVERABLE_MODE           GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define BLE_MIN_CONN_INTERVAL     800

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define BLE_MAX_CONN_INTERVAL     1600

// GAP Profile - Name attribute for SCAN RSP data
static uint8 scan_response[] =
{
	0x7,   // length of this data
	GAP_ADTYPE_LOCAL_NAME_COMPLETE,
	'N',
	'e',
	'o',
	'B',
	'a',
	'y',

	// connection interval range
	0x05,   // length of this data
	GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
	LO_UINT16( BLE_MIN_CONN_INTERVAL ),   // 100ms
	HI_UINT16( BLE_MIN_CONN_INTERVAL ),
	LO_UINT16( BLE_MAX_CONN_INTERVAL ),   // 1s
	HI_UINT16( BLE_MAX_CONN_INTERVAL ),

	// Tx power level
	GAP_ADTYPE_POWER_LEVEL,
	0       // 0dBm
};

// Advertisement data
static uint8 advertising_data[] =
{
	// Flags; this sets the device to use limited discoverable
	// mode (advertises for 30 seconds at a time) instead of general
	// discoverable mode (advertises indefinitely)
	0x02,   // length of this data
	GAP_ADTYPE_FLAGS,
	BLE_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

	// service UUID
	0x03,   // length of this data
	GAP_ADTYPE_16BIT_MORE,
	LO_UINT16( THERMOMETER_SERV_UUID ),
	HI_UINT16( THERMOMETER_SERV_UUID ),
};

// Device name attribute value
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = BLE_DEVICE_NAME;

unsigned short ble_get_gap_handle(void)
{
	struct ble_info *bi = &ble_info;

	return bi->gap_handle;
}

/*
 * Will be invoked when GAP state changed
 */
static void ble_gaprole_state_change(gaprole_States_t new_state)
{
	struct ble_info *bi = &ble_info;
	linkDBItem_t  *item;
	struct ble_status_change_msg *msg;

	print(LOG_INFO, MODULE "GAP: change to <%s>\n", gap_state_str[new_state]);

	msg = (struct ble_status_change_msg *)osal_msg_allocate(sizeof(struct ble_status_change_msg));
	if (!msg) {
		print(LOG_INFO, MODULE "fail to allocate <struct ble_status_change_msg>\n");
		return;
	}
	msg->hdr.event = BLE_STATUS_CHANGE_EVENT;

	switch (new_state) {
		case GAPROLE_CONNECTED:

			/* Get connection handle */
			GAPRole_GetParameter(GAPROLE_CONNHANDLE, &bi->gap_handle);

			item = linkDB_Find(bi->gap_handle);
			if (!item) {
				print(LOG_WARNING, MODULE "linkDB_Find() return NULL\n");
				break;
			}

			print(LOG_DBG, MODULE "peer addr %02x:%02x:%02x:%02x:%02x:%02x\n",
	    			item->addr[0], item->addr[1], item->addr[2],
					item->addr[3], item->addr[4], item->addr[5]);

			if(osal_memcmp(item->addr, bi->last_connected_addr, B_ADDR_LEN )) {
				print(LOG_DBG, MODULE "connect to last addr\n");
	        	bi->connect_to_last_addr = TRUE;
	        } else {
	        	osal_memcpy(bi->last_connected_addr, item->addr, B_ADDR_LEN );
	        }

			msg->type = BLE_CONNECT;
			osal_msg_send(bi->task_id, (uint8 *)msg);

			break;

		default:
			break;
	}

	if (bi->gap_role_state == GAPROLE_CONNECTED && new_state != GAPROLE_CONNECTED) {

		msg->type = BLE_DISCONNECT;
		osal_msg_send(bi->task_id, (uint8 *)msg);
	}

	bi->gap_role_state = new_state;

	return;
}

/*
 * When ther profile is written, this callback will be invoked
 *
 * We will report this event to ther app by osal_set_event()
 */
static void ble_thermometer_service(uint8 event)
{
	struct ble_info *bi = &ble_info;

	bi->handle_ts_event(event);
}

// GAP Role Callbacks
static gapRolesCBs_t ther_gaprole_hooks =
{
	ble_gaprole_state_change,	// Profile State Change Callbacks
	NULL              		// When a valid RSSI is read from controller
};

static void ble_gap_passcode(uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs)
{
	// Send passcode response
	GAPBondMgr_PasscodeRsp( connectionHandle, SUCCESS, 0 );
}


static void ble_gap_pair(uint16 connHandle, uint8 state, uint8 status)
{
}

// GAP Bond Manager Callbacks
static gapBondCBs_t ther_gap_bond_hooks =
{
	ble_gap_passcode,
	ble_gap_pair,
};

static void ble_start(void)
{
    // Start the Device
    VOID GAPRole_StartDevice( &ther_gaprole_hooks );

    // Register with bond manager after starting device
    VOID GAPBondMgr_Register( &ther_gap_bond_hooks );
}

static void ble_start_advertising(void)
{
	unsigned char val = TRUE;

	//change the GAP advertisement status
	GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &val);

	return;
}

/*
 * Set advertising interval
 * ms ??
 */
static void ble_set_adv_interval(unsigned short interval)
{
	GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, interval);
	GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, interval);

	GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MIN, interval);
	GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MAX, interval);
}

/*
 * Setup the GAP Peripheral Role Profile
 */
static void ble_setup_gap(struct ble_info *bi)
{
	GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &bi->advertising_enable);
	GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &bi->advertising_off_time);

	GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof(scan_response), scan_response );
	GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof(advertising_data), advertising_data );

	GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &bi->update_request_enable);
	GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &bi->min_connection_interval);
	GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &bi->max_connection_interval);
	GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &bi->slave_latency);
	GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &bi->connection_timeout);

	if (bi->advertising_enable)
		ble_set_adv_interval(2000);

	// Set the GAP Characteristics
	GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

	{
		  // Set connection parameters:
		  {
		    uint16 connectionInterval = 10;
//		    uint16 slaveLatency = APP_SLAVE_LATENCY;
//		    uint16 timeout = APP_CONN_TIMEOUT;

		    VOID GAP_SetParamValue( TGAP_CONN_EST_INT_MIN, connectionInterval );
		    VOID GAP_SetParamValue( TGAP_CONN_EST_INT_MAX, connectionInterval );
//		    VOID GAP_SetParamValue( TGAP_CONN_EST_LATENCY, slaveLatency );
//		    VOID GAP_SetParamValue( TGAP_CONN_EST_SUPERV_TIMEOUT, timeout );
		  }
	}
}

/*
 * Setup the GAP Bond Manager
 */
static void ble_set_gap_bond_mngt(struct ble_info *bi)
{
	uint32 passkey = 0; // passkey "000000"
	uint8 pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
	uint8 mitm = FALSE;
	uint8 ioCap = GAPBOND_IO_CAP_DISPLAY_ONLY;
	uint8 bonding = TRUE;

	GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof ( uint32 ), &passkey );
	GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof ( uint8 ), &pairMode );
	GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof ( uint8 ), &mitm );
	GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof ( uint8 ), &ioCap );
	GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof ( uint8 ), &bonding );
}

static void ble_init_thermometer_service(void)
{
	/*
	 * Setup the Thermometer Characteristic Values
	 */
	uint8 thermometerSite = THERMOMETER_TYPE_MOUTH;
	Thermometer_SetParameter( THERMOMETER_TYPE, sizeof ( uint8 ), &thermometerSite );

	thermometerIRange_t thermometerIRange= {4,60};
	Thermometer_SetParameter( THERMOMETER_IRANGE, sizeof ( uint16 ), &thermometerIRange );

	/* Register for Thermometer service callback */
	Thermometer_Register(ble_thermometer_service);

	Thermometer_AddService(GATT_ALL_SERVICES);
}

/*
 * 273 -> 2v
 * 313 -> 2.3v
 * 409 -> 3v
 */
#define BATT_ADC_MIN 313
#define BATT_ADC_MAX 409

static unsigned char ther_batt_cal_percentage(unsigned short adc)
{
	unsigned char percentage;

	if (adc >= BATT_ADC_MAX)
		percentage = 100;
	else if (adc <= BATT_ADC_MIN)
		percentage = 0;
	else
		percentage = (adc - BATT_ADC_MIN) * 100 / (BATT_ADC_MAX - BATT_ADC_MIN);

	return percentage;
}

static void ble_init_batt_service(void)
{
	Batt_Setup(HAL_ADC_CHANNEL_VDD, BATT_ADC_MIN, BATT_ADC_MAX, NULL, NULL, ther_batt_cal_percentage);

	Batt_AddService();
}

static void ble_init_devinfo(void)
{
	char fw_version[FW_STRING_LEN];

#if defined(POWER_SAVING)
	sprintf(fw_version, "V%d.%d(ps) %s",
			FIRMWARE_MAJOR_VERSION, FIREWARM_MINOR_VERSION, __DATE__);
#else
	sprintf(fw_version, "V%d.%d %s",
			FIRMWARE_MAJOR_VERSION, FIREWARM_MINOR_VERSION, __DATE__);
#endif
	DevInfo_SetParameter(DEVINFO_FIRMWARE_REV, sizeof(fw_version), fw_version);

	DevInfo_AddService();
}

void ble_private_service(uint8 event, uint8 *data, uint8 *len)
{
	struct ble_info *bi = &ble_info;

	bi->handle_ps_event(event, data, len);

	return;
}

static void ble_init_private_service(void)
{
	ther_add_private_service(GATT_ALL_SERVICES, ble_private_service);
}

unsigned char ther_ble_init(uint8 task_id, void (*handle_ts_event)(unsigned char event),
		void (*handle_ps_event)(unsigned char event, unsigned char *data, unsigned char *len))
{
	struct ble_info *bi = &ble_info;

	print(LOG_INFO, MODULE "ble init\n");

	bi->task_id = task_id;

	bi->handle_ts_event = handle_ts_event;
	bi->handle_ps_event = handle_ps_event;

	/* init param */
	bi->advertising_enable = TRUE;
	bi->advertising_off_time = 2000;
	bi->update_request_enable = TRUE;
	bi->min_connection_interval = BLE_MIN_CONN_INTERVAL;
	bi->max_connection_interval = BLE_MAX_CONN_INTERVAL;
	bi->slave_latency = 5;
	bi->connection_timeout = 1000;

	ble_setup_gap(bi);

	ble_set_gap_bond_mngt(bi);

	// Initialize GATT Client
	VOID GATT_InitClient();

	// Register to receive incoming ATT Indications/Notifications
	GATT_RegisterForInd(bi->task_id);

	// Initialize GATT attributes
	GGS_AddService( GATT_ALL_SERVICES );         // GAP
	GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes

	ble_init_devinfo();
	ble_init_thermometer_service();
	ble_init_batt_service();
	ble_init_private_service();

	ble_start();

	return 0;
}

void ther_ble_exit(void)
{
	struct ble_info *bi = &ble_info;

	bi->advertising_enable = FALSE;
	GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &bi->advertising_enable);

	if (bi->gap_role_state == GAPROLE_CONNECTED) {
		GAPRole_TerminateConnection();
	}
}

