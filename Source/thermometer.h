/**************************************************************************************************
  Filename:       thermometer.h
  Revised:        $Date: 2012-02-15 11:10:14 -0800 (Wed, 15 Feb 2012) $
  Revision:       $Revision: 29301 $

  Description:    This file contains the thermometer BLE sample application
                  definitions and prototypes.

  Copyright 2011 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

#ifndef THERMOMETER_H
#define THERMOMETER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */


// Thermomometer Task Events
#define TH_POWER_ON_EVT				0x0001
#define TH_POWER_OFF_EVT			0x0002
#define TH_BUZZER_EVT				0x0004
#define TH_BUTTON_EVT				0x0008
#define TH_TEMP_MEASURE_EVT			0x0010
#define TH_DISPLAY_EVT				0x0020
#define TH_WATCHDOG_EVT				0x0040
#define TH_BATT_EVT					0x0080
#define TH_HIS_TEMP_RESTORE_EVT		0x0100
#define TH_TEST_EVT					0x0800

enum {
	NORMAL_MODE = 0,
	CAL_MODE,
};

struct ther_info {
	uint8 task_id;

	unsigned char power_mode;

	bool ble_connect;

	/*
	 * mode
	 */
	unsigned char mode;



	/*
	 * Display
	 */
	unsigned char display_picture;

	/*
	 * Indication
	 */
	bool temp_indication_enable;
	unsigned char indication_interval; /* second */

	/*
	 * Notification
	 */
	bool temp_notification_enable;
	unsigned char notification_interval; /* second */

	/* temp */
	unsigned char temp_measure_stage;
	unsigned short temp_last_saved;
	unsigned short temp_current; /* every TEMP_MEASURE_INTERVAL */
	unsigned long temp_measure_interval;

	bool his_temp_uploading;
	uint8 *his_temp_bundle;
	uint16 his_temp_offset;
	uint16 his_temp_len;

	/* batt */
	unsigned char batt_percentage;

	/* private service */
	unsigned char warning_enabled;
	unsigned short high_temp; /* high warning temp */
};
extern struct ther_info ther_info;

struct ther_info *get_ti(void);

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * FUNCTIONS
 */
  
/*********************************************************************
 * GLOBAL VARIABLES
 */


/*
 * Task Initialization for the BLE Application
 */
extern void Thermometer_Init( uint8 task_id );

/*
 * Task Event Processor for the BLE Application
 */
extern uint16 Thermometer_ProcessEvent( uint8 task_id, uint16 events );



/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /*THERMOMETER_H */
