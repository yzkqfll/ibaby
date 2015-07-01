
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

#include "config.h"
#include "ther_uart.h"
#include "ther_uart_drv.h"

#include "ther_ble.h"

#include "ther_comm.h"

#include "ther_button.h"
#include "ther_buzzer.h"
#include "ther_oled9639_display.h"
#include "ther_mtd.h"
#include "ther_temp.h"
#include "ther_at.h"
#include "ther_misc.h"


#define MODULE "[THER] "

enum {
	PM_ACTIVE = 0,
	PM_IDLE,
	PM_1,
	PM_2,
	PM_3,
};

struct ther_info ther_info;

#define SEC_TO_MS(sec) ((sec) * 1000)

/**
 * Display
 */
#define DISPLAY_TIME SEC_TO_MS(5)
#define DISPLAY_WELCOME_TIME SEC_TO_MS(2)
#define DISPLAY_GOODBYE_TIME SEC_TO_MS(2)


/*
 * Power on/off
 */
#define SYSTEM_POWER_ON_DELAY 100 /* ms */
#define SYSTEM_POWER_OFF_TIME SEC_TO_MS(3)

/*
 * Temp measurement
 */
#define TEMP_POWER_SETUP_TIME 10 /* ms */
#define TEMP_MEASURE_INTERVAL SEC_TO_MS(5)
#define TEMP_MEASURE_MIN_INTERVAL SEC_TO_MS(1)

/*
 * Watchdog
 */
#define WATCHDOG_FEED_INTERVAL 500

static void ther_device_exit_pre(struct ther_info *ti);
static void ther_device_exit_post(struct ther_info *ti);
static void ther_system_power_on(struct ther_info *ti);
static void ther_system_power_off_pre(struct ther_info *ti);
static void ther_system_power_off_post(struct ther_info *ti);

struct ther_info *get_ti(void)
{
	return &ther_info;
}

static void change_measure_timer(struct ther_info *ti, unsigned long new_interval)
{
	if (ti->mode == NORMAL_MODE) {
		osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);

		ti->temp_measure_interval = new_interval;
		osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
	}
}

static void encap_first_picture_param(struct ther_info *ti, struct display_param *param)
{
	unsigned short time;
	UTCTimeStruct utc;

	osal_ConvertUTCTime(&utc, osal_getClock());
	time = (unsigned short)utc.hour << 8 | utc.minutes;

	param->picture = OLED_PICTURE1;
	param->remain_ms = DISPLAY_TIME;
	param->time = time;
	param->batt_level = 0;
	param->ble_link = LINK_ON;
	param->temp = ti->temp_current;
}

static void ther_handle_button(struct ther_info *ti, struct button_msg *msg)
{
	switch (msg->type) {
	case SHORT_PRESS:
		print(LOG_DBG, MODULE "button pressed\n");

		if (ti->power_mode == PM_ACTIVE) {

			if (ti->display_picture > OLED_PICTURE_NONE) {
				print(LOG_DBG, MODULE "ignore button press when picture is %d\n", ti->display_picture);
				break;
			}

			if (ti->display_picture == OLED_PICTURE_NONE) {
				struct display_param param;

				encap_first_picture_param(ti, &param);
				oled_show_picture(&param);

			} else {
				oled_show_next_picture(DISPLAY_TIME);
			}

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "ignore short press, power down !!!\n");
			ther_system_power_off_post(ti);
		} else {
			print(LOG_DBG, MODULE "unknown power mode\n");
		}

		break;

	case LONG_PRESS:
		print(LOG_DBG, MODULE "button long pressed\n");

		if (ti->power_mode == PM_ACTIVE) {
			ther_system_power_off_pre(ti);

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "power up in long press button\n");
			osal_start_timerEx(ti->task_id, TH_POWER_ON_EVT, SYSTEM_POWER_ON_DELAY);
			ti->power_mode = PM_ACTIVE;
		}

		break;

	default:
		print(LOG_WARNING, MODULE "unknow button press\n");
		break;
	}

	return;
}


static void ther_handle_gatt_access_msg(struct ther_info *ti, struct ble_gatt_access_msg *msg)
{
	switch (msg->type) {

	case GATT_TEMP_IND_ENABLED:
		print(LOG_INFO, MODULE "start temp indication\n");

		ti->temp_indication_enable = TRUE;
		ti->indication_interval = 5;

		break;

	case GATT_TEMP_IND_DISABLED:
		print(LOG_INFO, MODULE "stop temp indication\n");

		ti->temp_indication_enable = FALSE;

		break;

	case GATT_IMEAS_NOTI_ENABLED:
		print(LOG_INFO, MODULE "start imeas notification\n");

		ti->temp_notification_enable = TRUE;
		ti->notification_interval = 5;

		break;

	case GATT_IMEAS_NOTI_DISABLED:
		print(LOG_INFO, MODULE "stop imeas notification\n");
		ti->temp_notification_enable = FALSE;

		break;

	case GATT_INTERVAL_IND_ENABLED:
		print(LOG_INFO, MODULE "start interval indication\n");

		break;

	case GATT_INTERVAL_IND_DISABLED:
		print(LOG_INFO, MODULE "stop interval indication\n");

		break;

	case GATT_UNKNOWN:
		print(LOG_INFO, MODULE "unknown gatt access type\n");
		break;
	}

	return;
}

static void ther_handle_ble_status_change(struct ther_info *ti, struct ble_status_change_msg *msg)
{
	if (msg->type == BLE_DISCONNECT) {
		ti->temp_indication_enable = FALSE;
		ti->temp_notification_enable = FALSE;

		ti->ble_connect = FALSE;
	} else if (msg->type == BLE_CONNECT) {
		ti->ble_connect = TRUE;
	}

}

static void ther_dispatch_msg(struct ther_info *ti, osal_event_hdr_t *msg)
{
	switch (msg->event) {
	case USER_BUTTON_EVENT:
		ther_handle_button(ti, (struct button_msg *)msg);
		break;

	case GATT_MSG_EVENT:
		ther_handle_gatt_msg((gattMsgEvent_t *)msg);
		break;

	case BLE_GATT_ACCESS_EVENT:
		ther_handle_gatt_access_msg(ti, (struct ble_gatt_access_msg *)msg);
		break;

	case BLE_STATUS_CHANGE_EVENT:
		ther_handle_ble_status_change(ti, (struct ble_status_change_msg *)msg);
		break;

	default:
		break;
	}
}

static void ther_display_event_report(unsigned char event, unsigned short param)
{
	struct ther_info *ti = &ther_info;

	switch (event) {
	case OLED_EVENT_DISPLAY_ON:
		/* change temp measure to 1 sec */
		change_measure_timer(ti, TEMP_MEASURE_MIN_INTERVAL);
		ti->display_picture = param;
		break;

	case OLED_EVENT_TIME_TO_END:
		change_measure_timer(ti, TEMP_MEASURE_INTERVAL);
		if (ti->display_picture == OLED_PICTURE_WELCOME) {
			/* show first picture after welcome */
			struct display_param param;

			encap_first_picture_param(ti, &param);
			oled_show_picture(&param);
		} else {
			ti->display_picture = OLED_PICTURE_NONE;
			oled_display_power_off();
		}

		break;

	}


}

static void ther_device_init(struct ther_info *ti)
{
	/* uart init */
	ther_uart_init(UART_PORT_0, UART_BAUD_RATE_115200, ther_at_handle);
	print(LOG_INFO, "\n\n");
	print(LOG_INFO, "--------------\n");
	print(LOG_INFO, "  Firmware version %d.%d\n",
			FIRMWARE_MAJOR_VERSION, FIREWARM_MINOR_VERSION);
	print(LOG_INFO, "--------------\n");

	/* button init */
	ther_button_init(ti->task_id);

	/* buzzer init */
	ther_buzzer_init(ti->task_id);

	/* oled display init */
	oled_display_init(ti->task_id, ther_display_event_report);

	/* mtd */
	ther_mtd_init();

	/* temp init */
	ther_temp_init();

	/* ble init */
	ther_ble_init(ti->task_id);
}

static void ther_device_exit_pre(struct ther_info *ti)
{
	/* ble init */
	ther_ble_exit();
}

static void ther_device_exit_post(struct ther_info *ti)
{
	/* oled display exit */
	oled_display_exit();
}

static void ther_system_power_on(struct ther_info *ti)
{
	struct display_param param;

	ther_device_init(ti);

	/*
	 * play music
	 */
	ther_buzzer_play_music(BUZZER_MUSIC_SYS_POWER_ON);

	/*
	 * show welcome picture
	 */
	param.picture = OLED_PICTURE_WELCOME;
	param.remain_ms = DISPLAY_WELCOME_TIME;
	oled_show_picture(&param);

	/*
	 * start temp measurement
	 */
	ti->temp_measure_interval = TEMP_MEASURE_INTERVAL;
	ti->temp_measure_stage = TEMP_STAGE_SETUP;
	osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);

	/* test */
//	osal_start_timerEx(ti->task_id, TH_TEST_EVT, 5000);
}

static void ther_system_power_off_pre(struct ther_info *ti)
{
	struct display_param param;

	/* test */
	osal_stop_timerEx(ti->task_id, TH_TEST_EVT);

	/*
	 * stop temp measurement
	 */
	osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);

	/*
	 * play power off music
	 */
	ther_buzzer_stop_music();
	ther_buzzer_play_music(BUZZER_MUSIC_SYS_POWER_OFF);

	/*
	 * oled says goodbye
	 */
	param.picture = OLED_PICTURE_GOODBYE;
	param.remain_ms = DISPLAY_GOODBYE_TIME;
	oled_show_picture(&param);

	osal_start_timerEx(ti->task_id, TH_POWER_OFF_EVT, SYSTEM_POWER_OFF_TIME);

	ther_device_exit_pre(ti);
}

static void ther_system_power_off_post(struct ther_info *ti)
{
	ther_device_exit_post(ti);

	/*
	 * do not stop wd timer here,
	 * so the wd timer will be auto running after power on
	 */
/*	osal_stop_timerEx(ti->task_id, TH_WATCHDOG_EVT);*/

	ti->power_mode = PM_3;
	/* go to PM3 */
    SLEEPCMD |= BV(0) | BV(1);
    PCON |=BV(0);
}

/*********************************************************************
 * @fn      Thermometer_ProcessEvent
 *
 * @brief   Thermometer Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 Thermometer_ProcessEvent(uint8 task_id, uint16 events)
{
	struct ther_info *ti = &ther_info;

	/* message handle */
	if ( events & SYS_EVENT_MSG ) {
		uint8 *msg;

		if ( (msg = osal_msg_receive(ti->task_id)) != NULL ) {
			ther_dispatch_msg(ti, (osal_event_hdr_t *)msg);

			osal_msg_deallocate( msg );
		}

		return (events ^ SYS_EVENT_MSG);
	}

	if (events & TH_POWER_ON_EVT) {
		ther_system_power_on(ti);

		return (events ^ TH_POWER_ON_EVT);
	}

	if (events & TH_POWER_OFF_EVT) {
		ther_system_power_off_post(ti);

		return (events ^ TH_POWER_OFF_EVT);
	}

	/* temp measure event */
	if (events & TH_TEMP_MEASURE_EVT) {

		switch (ti->temp_measure_stage) {
		case TEMP_STAGE_SETUP:
			ther_temp_power_on();

			if (ti->mode == NORMAL_MODE) {
				osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);
				ti->temp_measure_stage = TEMP_STAGE_MEASURE;
			}
			break;

		case TEMP_STAGE_MEASURE:

			ti->temp_last_saved = ti->temp_current;
			ti->temp_current = ther_get_temp();
			ther_temp_power_off();

			if (ti->ble_connect) {
				bool music = TRUE;
				if (ti->temp_notification_enable) {
					ther_send_temp_notify(ble_get_gap_handle(), ti->temp_current);
				} else if (ti->temp_indication_enable) {
					ther_send_temp_indicate(ble_get_gap_handle(), ti->task_id, ti->temp_current);
				} else {
					music = FALSE;
				}

				if (music)
					ther_buzzer_play_music(BUZZER_MUSIC_SEND_TEMP);
			} else {
				// TODO: save to local
			}

			if (ti->display_picture == OLED_PICTURE1 &&
				ti->temp_current != ti->temp_last_saved) {
				oled_update_picture(OLED_PICTURE1, OLED_CONTENT_TEMP, ti->temp_current);
			}

			if (ti->mode == NORMAL_MODE)
				osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
			ti->temp_measure_stage = TEMP_STAGE_SETUP;
			break;

		default:
			break;
		}

		return (events ^ TH_TEMP_MEASURE_EVT);
	}

	/* Display event */
	if (events & TH_DISPLAY_EVT) {

		oled_display_state_machine();

		return (events ^ TH_DISPLAY_EVT);
	}

	/* buzzer event */
	if (events & TH_BUZZER_EVT) {
		ther_buzzer_check_music();

		return (events ^ TH_BUZZER_EVT);
	}

	/* button event */
	if (events & TH_BUTTON_EVT) {
		ther_measure_button_time();

		return (events ^ TH_BUTTON_EVT);
	}

	if (events & TH_WATCHDOG_EVT) {
		feed_watchdog();
		osal_start_timerEx(ti->task_id, TH_WATCHDOG_EVT, WATCHDOG_FEED_INTERVAL);

		return (events ^ TH_WATCHDOG_EVT);
	}

	if (events & TH_TEST_EVT) {
//		oled_picture_inverse();

		print(LOG_DBG, MODULE "live\n");

//		oled_show_temp(TRUE, ti->current_temp);

//		ther_spi_w25x_test(0,1,32);


//		print(LOG_DBG, "ADC0 %d\n", ther_get_adc(0));
//		print(LOG_DBG, "ADC1 %d\n", ther_get_adc(1));

		osal_start_timerEx(ti->task_id, TH_TEST_EVT, 1000);

		return (events ^ TH_TEST_EVT);
	}

	return 0;
}


/*********************************************************************
 * @fn      Thermometer_Init
 *
 * @brief   Initialization function for the Thermometer App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void Thermometer_Init(uint8 task_id)
{
	struct ther_info *ti = &ther_info;

	ti->task_id = task_id;

	ti->power_mode = PM_ACTIVE;

	start_watchdog_timer();
	osal_start_timerEx( ti->task_id, TH_WATCHDOG_EVT, WATCHDOG_FEED_INTERVAL);

	osal_start_timerEx(ti->task_id, TH_POWER_ON_EVT, SYSTEM_POWER_ON_DELAY);
}

/*
 * Just for compiling
 */
void HalLedEnterSleep(void) {}

/*
 * Just for compiling
 */
void HalLedExitSleep(void) {}
