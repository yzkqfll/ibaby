
#include "Comdef.h"
#include "OSAL.h"
#include "hal_board.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "thermometer.h"
#include "ther_button.h"

#define MODULE "[BUTTON] "


/*
 * P1.3 used as power key
 */
#define PUSH_BUTTON_PIN P1_3
#define PUSH_BUTTON_BIT 3

#define P1ICONL_BIT 1

#define BUTTON_MEASURE_INTERVAL 20

#define SHORT_PRESS_TIME 30
#define LONG_PRESS_TIME 1200

struct ther_button {
	unsigned char father_task_id;

	unsigned short eclipse_ms;
};

struct ther_button ther_button;

void ther_measure_button_time(void)
{
	struct ther_button *bt = &ther_button;
	struct button_msg *msg;

	bt->eclipse_ms += BUTTON_MEASURE_INTERVAL;

	if (bt->eclipse_ms > LONG_PRESS_TIME || (PUSH_BUTTON_PIN == 0)) {
		if (bt->eclipse_ms < SHORT_PRESS_TIME) {
			/*
			 * ignore debounce of the button
			 */
			print(LOG_DBG, MODULE "button pressed %d ms, ignore it\r\n", bt->eclipse_ms);
			return;
		}

		msg = (struct button_msg *)osal_msg_allocate(sizeof(struct button_msg));
		if (!msg) {
			print(LOG_ERR, MODULE "fail to allocate <struct button_msg>\r\n");
			return;
		}
		msg->hdr.event = USER_BUTTON_EVENT;

		if (bt->eclipse_ms > LONG_PRESS_TIME) {
			print(LOG_DBG, MODULE "button pressed %d ms, long press\r\n", bt->eclipse_ms);
			msg->type = LONG_PRESS;
		} else {
			print(LOG_DBG, MODULE "button pressed %d ms, short press\r\n", bt->eclipse_ms);
			msg->type = SHORT_PRESS;
		}

		osal_msg_send(bt->father_task_id, (uint8 *)msg);

		return;
	}

	osal_start_timerEx(bt->father_task_id, TH_BUTTON_EVT, BUTTON_MEASURE_INTERVAL);
	return;

}

void ther_button_init(unsigned char task_id)
{
	struct ther_button *bt = &ther_button;

	print(LOG_INFO, MODULE "button init\r\n");

	bt->father_task_id = task_id;

	/*
	 * P1.3 is push button
	 *   gpio, output, interrupt triggered(Port 1 vector), rising edge
	 */

	/* GPIO, and as input */
	P1SEL &= ~BV(PUSH_BUTTON_BIT);
	P1DIR &= ~BV(PUSH_BUTTON_BIT);

	/* 3-state */
	P1INP |= BV(PUSH_BUTTON_BIT);

	/* rising edge */
	PICTL &= ~BV(P1ICONL_BIT);

	/* Port 1 interrupt enable */
	IEN2 |= BV(4);

	/* enable P1.3 interrupt and clear the status flag */
	P1IEN |= BV(PUSH_BUTTON_BIT);
	P1IFG &= ~BV(PUSH_BUTTON_BIT);

}

/*
#pragma vector = P1INT_VECTOR
__interrupt void P1_ISR(void)
*/
HAL_ISR_FUNCTION(button_isr, P1INT_VECTOR)
{
	HAL_ENTER_ISR();

	if (P1IFG & BV(PUSH_BUTTON_BIT)) {
		struct ther_button *bt = &ther_button;

		P1IFG &= ~BV(PUSH_BUTTON_BIT);

		osal_stop_timerEx(bt->father_task_id, TH_BUTTON_EVT);
		osal_start_timerEx(bt->father_task_id, TH_BUTTON_EVT, BUTTON_MEASURE_INTERVAL);
		bt->eclipse_ms = 0;
	}
	/* clear P1 interrupt pending flag */
	P1IF = 0;

	CLEAR_SLEEP_MODE();

	HAL_EXIT_ISR();

	return;
}
