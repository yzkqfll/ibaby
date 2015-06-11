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

#include "battservice.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "ther_ble.h"

#define MODULE "[THER BLE] "

struct ther_ble_info {
	uint8 task_id;

	uint16 gap_handle;
	gaprole_States_t gap_role_state;

	uint8 last_connected_addr[B_ADDR_LEN];
	bool connect_to_last_addr;
};

static struct ther_ble_info ble_info;

/*
 * If it consumes a lot of memory,
 * directly use string instead of static variable
 */
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
#define DEFAULT_DISCOVERABLE_MODE           GAP_ADTYPE_FLAGS_GENERAL

// How often to perform periodic event
#define PERIODIC_EVT_PERIOD                   1000

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         FALSE

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     200

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     1600

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         1

// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          1000

// Delay to begin discovery from start of connection in ms
#define DEFAULT_DISCOVERY_DELAY               1000

// GAP Profile - Name attribute for SCAN RSP data
static uint8 scanResponseData[] =
{
  0x12,   // length of this data
  GAP_ADTYPE_LOCAL_NAME_COMPLETE,
  '0',
  'h',
  'e',
  'r',
  'm',
  'o',
  'm',
  'e',
  't',
  'e',
  'r',
  'S',
  'e',
  'n',
  's',
  'o',
  'r',
    // connection interval range
  0x05,   // length of this data
  GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
  LO_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),   // 100ms
  HI_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),
  LO_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),   // 1s
  HI_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),

  // Tx power level
  GAP_ADTYPE_POWER_LEVEL,
  0       // 0dBm
};

// Advertisement data
static uint8 advertData[] =
{
  // Flags; this sets the device to use limited discoverable
  // mode (advertises for 30 seconds at a time) instead of general
  // discoverable mode (advertises indefinitely)
  0x02,   // length of this data
  GAP_ADTYPE_FLAGS,
  DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

  // service UUID
  0x03,   // length of this data
  GAP_ADTYPE_16BIT_MORE,
  LO_UINT16( THERMOMETER_SERV_UUID ),
  HI_UINT16( THERMOMETER_SERV_UUID ),

};

// Device name attribute value
static uint8 attDeviceName[GAP_DEVICE_NAME_LEN] = "0hermometer Sensor";

unsigned short ble_get_gap_handle(void)
{
	struct ther_ble_info *bi = &ble_info;

	return bi->gap_handle;
}

void ble_start_advertise(void)
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
static void ble_set_adv_interval(unsigned long interval)
{

	GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, interval);
	GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, interval);

	GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MIN, interval);
	GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MAX, interval);
}

/*
 * Will be invoked when GAP state changed
 */
static void ble_gap_role_new_state(gaprole_States_t new_state)
{
	struct ther_ble_info *bi = &ble_info;
	linkDBItem_t  *item;
	struct ble_status_change_msg *msg;

	print(LOG_INFO, MODULE "GAP: change to <%s>\r\n", gap_state_str[new_state]);

	msg = (struct ble_status_change_msg *)osal_msg_allocate(sizeof(struct ble_status_change_msg));
	if (!msg) {
		print(LOG_INFO, MODULE "fail to allocate <struct ble_status_change_msg>\r\n");
		return;
	}
	msg->hdr.event = BLE_STATUS_CHANGE_EVENT;

	switch (new_state) {
		case GAPROLE_CONNECTED:

			/* Get connection handle */
			GAPRole_GetParameter(GAPROLE_CONNHANDLE, &bi->gap_handle);

			item = linkDB_Find(bi->gap_handle);
			if (!item) {
				print(LOG_WRANING, MODULE "linkDB_Find() return NULL\r\n");
				break;
			}

	    	print(LOG_DBG, MODULE "peer addr %02x:%02x:%02x:%02x:%02x:%02x\r\n",
	    			item->addr[0], item->addr[1], item->addr[2],
					item->addr[3], item->addr[4], item->addr[5]);

	        if(osal_memcmp(item->addr, bi->last_connected_addr, B_ADDR_LEN )) {
	        	print(LOG_DBG, MODULE "connect to last addr\r\n");
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
static void ble_gatt_accessed(uint8 event)
{
	struct ther_ble_info *bi = &ble_info;
	struct ble_gatt_access_msg *msg;

	msg = (struct ble_gatt_access_msg *)osal_msg_allocate(sizeof(struct ble_gatt_access_msg));
	if (!msg) {
		print(LOG_INFO, MODULE "fail to allocate <struct ble_msg>\r\n");
		return;
	}

	msg->hdr.event = BLE_GATT_ACCESS_EVENT;

	switch (event) {
		case THERMOMETER_TEMP_IND_ENABLED:
			msg->type = GATT_TEMP_IND_ENABLED;
			break;

		case  THERMOMETER_TEMP_IND_DISABLED:
			msg->type = GATT_TEMP_IND_DISABLED;
			break;

		case THERMOMETER_IMEAS_NOTI_ENABLED:
			msg->type = GATT_IMEAS_NOTI_ENABLED;
			break;

		case  THERMOMETER_IMEAS_NOTI_DISABLED:
			msg->type = GATT_IMEAS_NOTI_DISABLED;
			break;

		case THERMOMETER_INTERVAL_IND_ENABLED:
			msg->type = GATT_INTERVAL_IND_ENABLED;
			break;

		case THERMOMETER_INTERVAL_IND_DISABLED:
			msg->type = GATT_INTERVAL_IND_DISABLED;
			break;

		default:
			msg->type = GATT_UNKNOWN;
			break;
	}

	osal_msg_send(bi->task_id, (uint8 *)msg);

	return;
}

// GAP Role Callbacks
static gapRolesCBs_t ther_gap_hooks =
{
  ble_gap_role_new_state,	// Profile State Change Callbacks
  NULL              		// When a valid RSSI is read from controller
};

static void ble_gap_passcode( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs )
{
  // Send passcode response
  GAPBondMgr_PasscodeRsp( connectionHandle, SUCCESS, 0 );
}


static void ble_gap_paring( uint16 connHandle, uint8 state, uint8 status )
{
}

// GAP Bond Manager Callbacks
static gapBondCBs_t ther_gap_bond_hooks =
{
  ble_gap_passcode,
  ble_gap_paring,
};

static void ble_start(void)
{
    // Start the Device
    VOID GAPRole_StartDevice( &ther_gap_hooks );

    // Register with bond manager after starting device
    VOID GAPBondMgr_Register( &ther_gap_bond_hooks );
}

unsigned char ther_ble_init(uint8 task_id)
{
	struct ther_ble_info *bi = &ble_info;

	print(LOG_INFO, MODULE "ble init ...\r\n");

	bi->task_id = task_id;

	/* Use for testing if chip default is 0xFFFFFF
	static uint8 bdAddress[6] = {0x1,0x2,0x3,0x4,0x5,0x6};
	HCI_EXT_SetBDADDRCmd(bdAddress);
	*/

	// Setup the GAP Peripheral Role Profile
	{
		// Device doesn't start advertising until button is pressed
		uint8 initial_advertising_enable = TRUE;

		// By setting this to zero, the device will go into the waiting state after
		// being discoverable for 30.72 second, and will not being advertising again
		// until the enabler is set back to TRUE
		uint16 gapRole_AdvertOffTime = 2000;

		uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
		uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
		uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
		uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
		uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;

		// Set the GAP Role Parameters
		GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );
		GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &gapRole_AdvertOffTime );

		GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanResponseData ), scanResponseData );
		GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof( advertData ), advertData );

		GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &enable_update_request );
		GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &desired_min_interval );
		GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &desired_max_interval );
		GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &desired_slave_latency );
		GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &desired_conn_timeout );
	}

	ble_set_adv_interval(2000);
	if (0)
	{
		/*
		* Change broadcast internal
		*/

		// Slow advertising interval in 625us units
		#define DEFAULT_SLOW_ADV_INTERVAL             1600

		// Duration of slow advertising duration in ms (set to 0 for continuous advertising)
		#define DEFAULT_SLOW_ADV_DURATION             0

		GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MIN, DEFAULT_SLOW_ADV_INTERVAL );
		GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MAX, DEFAULT_SLOW_ADV_INTERVAL );
		GAP_SetParamValue( TGAP_LIM_ADV_TIMEOUT, 30000 );
	}

	// Set the GAP Characteristics
	GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName );

	// Setup the GAP Bond Manager
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

	// Setup the Thermometer Characteristic Values
	{
		uint8 thermometerSite = THERMOMETER_TYPE_MOUTH;
		Thermometer_SetParameter( THERMOMETER_TYPE, sizeof ( uint8 ), &thermometerSite );

		thermometerIRange_t thermometerIRange= {4,60};
		Thermometer_SetParameter( THERMOMETER_IRANGE, sizeof ( uint16 ), &thermometerIRange );
	}


	// Initialize GATT Client
	VOID GATT_InitClient();

	// Register to receive incoming ATT Indications/Notifications
	GATT_RegisterForInd( bi->task_id );

	// Initialize GATT attributes
	GGS_AddService( GATT_ALL_SERVICES );         // GAP
	GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes
	Thermometer_AddService( GATT_ALL_SERVICES );
	DevInfo_AddService( );

//	Batt_AddService( );

	// Register for Thermometer service callback
	Thermometer_Register ( ble_gatt_accessed );

	ble_start();

	return 0;
}
