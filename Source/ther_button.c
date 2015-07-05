/*
 * THER BUTTON
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


#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"

#include "thermometer.h"
#include "ther_button.h"
#include "ther_port.h"

#define MODULE "[THER BUTTON] "

#define BUTTON_MEASURE_INTERVAL 20

#define SHORT_PRESS_TIME 30
#define LONG_PRESS_TIME 1200

struct ther_button {
	unsigned char task_id;

	unsigned short eclipse_ms;
};

struct ther_button ther_button;

void ther_measure_button_time(void)
{
	struct ther_button *bt = &ther_button;
	struct button_msg *msg;

	bt->eclipse_ms += BUTTON_MEASURE_INTERVAL;

	if (bt->eclipse_ms > LONG_PRESS_TIME || (P1_BUTTON_PIN == 0)) {
		if (bt->eclipse_ms < SHORT_PRESS_TIME) {
			/*
			 * ignore debounce of the button
			 */
			print(LOG_DBG, MODULE "button pressed %d ms, ignore it\n", bt->eclipse_ms);
			return;
		}

		msg = (struct button_msg *)osal_msg_allocate(sizeof(struct button_msg));
		if (!msg) {
			print(LOG_ERR, MODULE "fail to allocate <struct button_msg>\n");
			return;
		}
		msg->hdr.event = USER_BUTTON_EVENT;

		if (bt->eclipse_ms > LONG_PRESS_TIME) {
			print(LOG_DBG, MODULE "button pressed %d ms, long press\n", bt->eclipse_ms);
			msg->type = LONG_PRESS;
		} else {
			print(LOG_DBG, MODULE "button pressed %d ms, short press\n", bt->eclipse_ms);
			msg->type = SHORT_PRESS;
		}

		osal_msg_send(bt->task_id, (uint8 *)msg);

		return;
	}

	osal_start_timerEx(bt->task_id, TH_BUTTON_EVT, BUTTON_MEASURE_INTERVAL);
	return;

}

void ther_button_init(unsigned char task_id)
{
	struct ther_button *bt = &ther_button;

	print(LOG_INFO, MODULE "button init\n");

	bt->task_id = task_id;

}

/*
#pragma vector = P1INT_VECTOR
__interrupt void P1_ISR(void)
*/
HAL_ISR_FUNCTION(button_isr, P1INT_VECTOR)
{
	HAL_ENTER_ISR();

	if (P1IFG & BV(P1_BUTTON_BIT)) {
		struct ther_button *bt = &ther_button;

		P1IFG &= ~BV(P1_BUTTON_BIT);

		osal_stop_timerEx(bt->task_id, TH_BUTTON_EVT);
		osal_start_timerEx(bt->task_id, TH_BUTTON_EVT, BUTTON_MEASURE_INTERVAL);
		bt->eclipse_ms = 0;
	}
	/* clear P1 interrupt pending flag */
	P1IF = 0;

	CLEAR_SLEEP_MODE();

	HAL_EXIT_ISR();

	return;
}
