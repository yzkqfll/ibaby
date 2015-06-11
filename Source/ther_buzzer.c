/*
 * Copyright
 */

#include "Comdef.h"
#include "OSAL.h"
#include "hal_drivers.h"

#include "ther_uart.h"
#include "ther_uart_comm.h"

#include "hal_board.h"
#include "ther_buzzer.h"
#include "thermometer.h"

#define MODULE "[BUZZER] "

/*
 * GPIO P1.0 as buzzer driver
 * P1.0 do not have pullup/pulldown resistor
 *
 * timer4, PWM
 * 20mA max
 *
 * Registers
 *     P1SEL -> set as peripheral I/O signal(1: peripheral I/O)
 *     P1DIR -> set the direction(1: output)
 *     P1_0  -> value of P1.0
 *
 * Questions:
 *
 * Chapter 7.2   Low I/O Supply Voltage
 *
 */

/*
 * timer 4
 */
#define BUZZER_PIN P1_0
#define BUZZER_BIT 0  /* P1.0 */

/* Defines for Timer 4 */
#define HAL_T4_CC0_VALUE                125   /* provides pulse width of 125 usec */
#define HAL_T4_TIMER_CTL_DIV32          0xA0  /* Clock pre-scaled by 32 */
#define HAL_T4_TIMER_CTL_START          0x10
#define HAL_T4_TIMER_CTL_CLEAR          0x04
#define HAL_T4_TIMER_CTL_OPMODE_MODULO  0x02  /* Modulo Mode, Count from 0 to CompareValue */
#define HAL_T4_TIMER_CCTL_MODE_COMPARE  0x04
#define HAL_T4_TIMER_CCTL_CMP_TOGGLE    0x10
#define HAL_T4_TIMER_CTL_DIV64          0xC0  /* Clock pre-scaled by 64 */

enum {
	TONE_LOW = 0,
	TONE_MID,
	TONE_HIGH,
};

struct ther_buzzer {
	unsigned char task_id;

	unsigned char music_book;
	unsigned char cur_step;
};
struct ther_buzzer buzzer;

/*
 * music book
 */
#define TONE_OFFSET 0
#define LEN 5
static const unsigned short music_books[BUZZER_MUSIC_NR][LEN] =  {
	TONE_HIGH, 300, 200, 200, 100, /* sys boot */
	TONE_LOW, 400, 200, 300, 100,  /* send temp */
	TONE_LOW, 400, 200, 300, 100,  /* warning */
};

static void start_buzzer(unsigned char tone)
{
	switch (tone) {
	case TONE_LOW:
	    /* Buzzer is "rung" by using T4, channel 0 to generate 2kHz square wave */
	    T4CTL = HAL_T4_TIMER_CTL_DIV64 |
	            HAL_T4_TIMER_CTL_CLEAR |
	            HAL_T4_TIMER_CTL_OPMODE_MODULO;

		break;

	case TONE_MID:
	    /* Buzzer is "rung" by using T4, channel 0 to generate 2kHz square wave */
	    T4CTL = HAL_T4_TIMER_CTL_DIV64 |
	            HAL_T4_TIMER_CTL_CLEAR |
	            HAL_T4_TIMER_CTL_OPMODE_MODULO;
		break;

	case TONE_HIGH:
	    /* Buzzer is "rung" by using T4, channel 0 to generate 4kHz square wave */
	    T4CTL = HAL_T4_TIMER_CTL_DIV32 |
	            HAL_T4_TIMER_CTL_CLEAR |
	            HAL_T4_TIMER_CTL_OPMODE_MODULO;

		break;

	default:
		break;
	}

	T4CCTL0 = HAL_T4_TIMER_CCTL_MODE_COMPARE | HAL_T4_TIMER_CCTL_CMP_TOGGLE;
	T4CC0 = HAL_T4_CC0_VALUE;

	/* Start it */
	T4CTL |= HAL_T4_TIMER_CTL_START;
}

/*
 * stop_buzzer()
 */
static void stop_buzzer(void)
{
	BUZZER_PIN = 0;

	/* Setting T4CTL to 0 disables it and masks the overflow interrupt */
	T4CTL = 0;

	/* Return output pin to GPIO */
//	P1SEL &= (uint8) ~HAL_BUZZER_P1_GPIO_PINS;
}

void ther_play_music(unsigned char music_book)
{
	struct ther_buzzer *b = &buzzer;

//	print(LOG_DBG, MODULE "play music %d\r\n", music);

	b->music_book = music_book;
	b->cur_step = 0;

	osal_start_timerEx(b->task_id, TH_BUZZER_EVT, 200);
}

void ther_stop_music(void)
{
	struct ther_buzzer *b = &buzzer;

	print(LOG_DBG, MODULE "stop music %d\r\n");

	if (b->cur_step) {
		osal_stop_timerEx(b->task_id, TH_BUZZER_EVT);

		stop_buzzer();
		b->cur_step = 1;
	}
}

void ther_buzzer_play_music(void)
{
	struct ther_buzzer *b = &buzzer;
	unsigned char tone;

	if (b->cur_step < LEN) {
		if (b->cur_step % 2 != 0) {
			/* buzzer */
			tone = music_books[b->music_book][TONE_OFFSET];
			start_buzzer(tone);
		} else {
			/* slient */
			stop_buzzer();
		}

		osal_start_timerEx(b->task_id, TH_BUZZER_EVT, music_books[b->music_book][b->cur_step]);

		b->cur_step++;
	} else {
		b->cur_step = 1;
	}
}

void ther_buzzer_init(unsigned char task_id)
{
	struct ther_buzzer *b = &buzzer;

	b->task_id = task_id;

	BUZZER_PIN = 0;

	/* dir: output */
	P1DIR |= BV(BUZZER_BIT);

	/* as peripheral */
	P1SEL |= BV(BUZZER_BIT);
}
