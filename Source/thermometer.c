
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

#include "ther_ble.h"

#include "ther_comm.h"

#include "ther_button.h"
#include "ther_buzzer.h"
#include "ther_oled9639_display.h"
#include "ther_spi_w25x40cl.h"
#include "ther_temp.h"

#define MODULE "[THER] "

enum {
	PM_ACTIVE = 0,
	PM_IDLE,
	PM_1,
	PM_2,
	PM_3,
};

struct ther_info {
	uint8 task_id;

	unsigned char power_mode;

	bool ble_connect;

	/*
	 * Display
	 */
	unsigned char display_picture;
	unsigned short display_time;

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
	unsigned char temp_stage;
	unsigned short temp_last_saved;
	unsigned short temp_current; /* every TEMP_MEASURE_INTERVAL */
	unsigned long temp_measure_interval;
	bool has_history_temp;
};

static struct ther_info ther_info;

#define SEC_TO_MS(sec) ((sec) * 1000)

/**
 * Display
 */
#define DISPLAY_POWER_SETUP_TIME 40 /* ms */
#define DISPLAY_SWITCH_INTERVAL 40 /* ms */
#define DISPLAY_TIME SEC_TO_MS(5)
#define DISPLAY_WELCOME_TIME SEC_TO_MS(2)

/*
 * Temp measurement
 */
#define TEMP_POWER_SETUP_TIME 100 /* ms */
#define TEMP_MEASURE_INTERVAL SEC_TO_MS(5)
#define TEMP_MEASURE_MIN_INTERVAL SEC_TO_MS(2)

static void ther_temp_periodic_meas(struct ther_info *ti)
{
	/* need wait for response?? */
	ther_send_temp_indicate(ble_get_gap_handle(), ti->task_id, 0);

	if (ti->temp_indication_enable)
		osal_start_timerEx( ti->task_id, TH_PERIODIC_MEAS_EVT, SEC_TO_MS(ti->indication_interval));

	return;
}

static void ther_temp_periodic_imeas(struct ther_info *ti)
{
	ther_send_temp_notify(ble_get_gap_handle(), 0);

	if (ti->temp_notification_enable)
		osal_start_timerEx( ti->task_id, TH_PERIODIC_IMEAS_EVT, SEC_TO_MS(ti->notification_interval));

	return;
}

static void ther_handle_gatt_access_msg(struct ther_info *ti, struct ble_gatt_access_msg *msg)
{
	switch (msg->type) {

	case GATT_TEMP_IND_ENABLED:
		print(LOG_INFO, MODULE "start temp indication\r\n");

		ti->temp_indication_enable = TRUE;
		ti->indication_interval = 5;
		osal_start_timerEx(ti->task_id, TH_PERIODIC_MEAS_EVT, SEC_TO_MS(1));

		break;

	case GATT_TEMP_IND_DISABLED:
		print(LOG_INFO, MODULE "stop temp indication\r\n");

		ti->temp_indication_enable = FALSE;

		break;

	case GATT_IMEAS_NOTI_ENABLED:
		print(LOG_INFO, MODULE "start imeas notification\r\n");

		ti->temp_notification_enable = TRUE;
		ti->notification_interval = 5;
		osal_start_timerEx(ti->task_id, TH_PERIODIC_IMEAS_EVT, SEC_TO_MS(3));

		break;

	case GATT_IMEAS_NOTI_DISABLED:
		print(LOG_INFO, MODULE "stop imeas notification\r\n");
		ti->temp_notification_enable = FALSE;

		break;

	case GATT_INTERVAL_IND_ENABLED:
		print(LOG_INFO, MODULE "start interval indication\r\n");

		break;

	case GATT_INTERVAL_IND_DISABLED:
		print(LOG_INFO, MODULE "stop interval indication\r\n");

		break;

	case GATT_UNKNOWN:
		print(LOG_INFO, MODULE "unknown gatt access type\r\n");
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


static void ther_power_off(struct ther_info *ti)
{
//	oled_power_off();

	ti->power_mode = PM_3;

	/* go to PM3 */
    SLEEPCMD |= BV(0) | BV(1);
    PCON |=BV(0);

    return;
}

static void ther_power_on(struct ther_info *ti)
{
	ti->power_mode = PM_ACTIVE;

	/* uart init */
	uart_comm_init();

	/* button init */
	ther_button_init(ti->task_id);

	/* buzzer init */
	ther_buzzer_init(ti->task_id);
	ther_play_music(BUZZER_MUSIC_SYS_BOOT);

	/* oled display init */
	oled_display_init();

	return;
}

static void restart_measure_timer(struct ther_info *ti, unsigned long new_interval)
{
	osal_stop_timerEx(ti->task_id, TH_TEMP_MEASURE_EVT);

	ti->temp_measure_interval = new_interval;
	osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
}

static void ther_handle_button(struct ther_info *ti, struct button_msg *msg)
{
	switch (msg->type) {
	case SHORT_PRESS:
		print(LOG_DBG, MODULE "user press button\r\n");

		if (ti->power_mode == PM_ACTIVE) {

			if (ti->display_picture == OLED_DISPLAY_OFF) {
				/*
				 * oled need to be powered on 10ms before operating on it
				 */
				oled_power_on();
				ti->display_picture = OLED_DISPLAY_PICTURE1;
				ti->display_time = DISPLAY_TIME;
				osal_start_timerEx(ti->task_id, TH_DISPLAY_EVT, DISPLAY_POWER_SETUP_TIME);

				/* change temp measure to 1 sec */
				restart_measure_timer(ti, TEMP_MEASURE_MIN_INTERVAL);
			} else {

				/*
				 * switch to next picture
				 */
				ti->display_picture = (ti->display_picture + 1) % OLED_DISPLAY_MAX_PICTURE;
				ti->display_time = DISPLAY_TIME;

				osal_stop_timerEx(ti->task_id, TH_DISPLAY_EVT);
				osal_start_timerEx(ti->task_id, TH_DISPLAY_EVT, DISPLAY_SWITCH_INTERVAL);
			}

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "ignore short press, power down !!!\r\n");
			ther_power_off(ti);
		} else {
			print(LOG_DBG, MODULE "unknown power mode\r\n");
		}

		break;

	case LONG_PRESS:
//		print(LOG_DBG, MODULE "user long press button\r\n");

		if (ti->power_mode == PM_ACTIVE) {
			print(LOG_DBG, MODULE "power down !!!\r\n");
			ther_power_off(ti);

		} else if (ti->power_mode == PM_3) {
			print(LOG_DBG, MODULE "power up in long press button\r\n");
			ther_power_on(ti);
		}

		break;

	default:
		print(LOG_WRANING, MODULE "unknow button press\r\n");
		break;
	}

	return;
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

/**
 * The reason for the complex logic is to save power
 */
static void ther_display_show_picture(struct ther_info *ti)
{
	oled_clear_screen();

	switch (ti->display_picture) {
	case OLED_DISPLAY_WELCOME:
		oled_show_welcome();
		break;

	case OLED_DISPLAY_GOODBYE:
		oled_show_goodbye();
		break;

	case OLED_DISPLAY_PICTURE1:
		oled_show_first_picture(0, LINK_ON, 0, ti->temp_current);
		break;

	case OLED_DISPLAY_PICTURE2:
		oled_show_second_picture();
		break;

	default:
		print(LOG_WRANING, MODULE "unknown picture to shown!\r\n");
		break;
	}

}

static void ther_display_update_temp(struct ther_info *ti)
{
	if (ti->display_picture == OLED_DISPLAY_PICTURE1 &&
		ti->temp_current != ti->temp_last_saved) {
		oled_update_first_picture(OLED_CONTENT_TEMP, ti->temp_current);
	}
}

static void ther_init_device(struct ther_info *ti)
{
	/* uart init */
	uart_comm_init();
	print(LOG_INFO, "\r\n\r\n");
	print(LOG_INFO, "--------------\r\n");

	/* button init */
	ther_button_init(ti->task_id);

	/* buzzer init */
	ther_buzzer_init(ti->task_id);
	ther_play_music(BUZZER_MUSIC_SYS_BOOT);

	/* oled display init */
	oled_display_init();
	ti->display_picture = OLED_DISPLAY_OFF;

	/* spi flash */
	ther_spi_w25x_init();

	/* temp init */
	ther_temp_init();
	ti->temp_measure_interval = TEMP_MEASURE_INTERVAL;
	ti->temp_stage = TEMP_STAGE_SETUP;

	/* ble init */
	ther_ble_init(ti->task_id);

}


static void ther_start_system(struct ther_info *ti)
{

	ther_init_device(ti);

	/* test */
	osal_start_timerEx(ti->task_id, TH_TEST_EVT, 5000);

	/*
	 * show welcome picture
	 */
	oled_power_on();
	ti->display_picture = OLED_DISPLAY_WELCOME;
	ti->display_time = DISPLAY_WELCOME_TIME;
	osal_start_timerEx(ti->task_id, TH_DISPLAY_EVT, DISPLAY_POWER_SETUP_TIME);

	osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);

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

	if (events & TH_START_SYSTEM_EVT) {
		ther_start_system(ti);

		return (events ^ TH_START_SYSTEM_EVT);
	}

	/* temp measure event */
	if (events & TH_TEMP_MEASURE_EVT) {

		switch (ti->temp_stage) {
		case TEMP_STAGE_SETUP:
			ther_temp_power_on();

			osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, TEMP_POWER_SETUP_TIME);
			ti->temp_stage = TEMP_STAGE_MEASURE;
			break;

		case TEMP_STAGE_MEASURE:

			ti->temp_last_saved = ti->temp_current;
			ti->temp_current = ther_get_current_temp();

			if (ti->ble_connect) {
				if (ti->temp_notification_enable)
					ther_send_temp_notify(ble_get_gap_handle(), ti->temp_current);
				if (ti->temp_indication_enable)
					ther_send_temp_indicate(ble_get_gap_handle(), ti->task_id, ti->temp_current);
			} else {
				// TODO: save to local
			}

			if (ti->display_picture < OLED_DISPLAY_MAX_PICTURE) {
				/* update temp */
				ther_display_update_temp(ti);
			}

			osal_start_timerEx( ti->task_id, TH_TEMP_MEASURE_EVT, ti->temp_measure_interval);
			ti->temp_stage = TEMP_STAGE_SETUP;
			break;

		default:
			break;
		}

		return (events ^ TH_TEMP_MEASURE_EVT);
	}

	/* Display event */
	if (events & TH_DISPLAY_EVT) {

		if (ti->display_time) {
			ther_display_show_picture(ti);

			osal_start_timerEx( ti->task_id, TH_DISPLAY_EVT, ti->display_time);
			ti->display_time = 0;
		} else {
			ti->display_picture = OLED_DISPLAY_OFF;
			oled_power_off();

			/* change temp measure interval to 5 sec */
			restart_measure_timer(ti, TEMP_MEASURE_INTERVAL);
		}

		return (events ^ TH_DISPLAY_EVT);
	}

	if(events & TH_PERIODIC_MEAS_EVT) {
		ther_play_music(BUZZER_MUSIC_SEND_TEMP);

//		ther_temp_periodic_meas(ti);

		return (events ^ TH_PERIODIC_MEAS_EVT);
	}

	if (events & TH_PERIODIC_IMEAS_EVT) {
//		ther_temp_periodic_imeas(ti);

		return (events ^ TH_PERIODIC_IMEAS_EVT);
	}

	/* buzzer event */
	if (events & TH_BUZZER_EVT) {
		ther_buzzer_play_music();

		return (events ^ TH_BUZZER_EVT);
	}

	/* button event */
	if (events & TH_BUTTON_EVT) {
		ther_measure_button_time();

		return (events ^ TH_BUTTON_EVT);
	}

	if (events & TH_TEST_EVT) {
//		oled_picture_inverse();

//		print(LOG_DBG, MODULE "live\r\n");

//		oled_show_temp(TRUE, ti->current_temp);

		ther_spi_w25x_test();

		osal_start_timerEx(ti->task_id, TH_TEST_EVT, 5000);

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

	osal_start_timerEx(ti->task_id, TH_START_SYSTEM_EVT, 200);
}

void HalLedEnterSleep(void)
{

}
void HalLedExitSleep(void)
{

}
