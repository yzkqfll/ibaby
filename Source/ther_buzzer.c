/*
 * THER BUZZER
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
#include "hal_drivers.h"
#include "OSAL_PwrMgr.h"
#include "OSAL.H"

#include "ther_uart.h"

#include "hal_board.h"
#include "ther_buzzer.h"
#include "thermometer.h"
#include "ther_port.h"

#define MODULE "[BUZZER ] "

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

	unsigned char music;
	unsigned char cur_step;
	bool pwm_on;
};
static struct ther_buzzer buzzer;

/*
 * music book
 */
#define LEN 5

#define TONE_OFFSET 0
#define STEP_START 1
#define STEP_END (LEN - 1)

static const unsigned short musics[BUZZER_MUSIC_NR][LEN] =  {
	TONE_HIGH, 300, 200, 200, 100, /* power on */
	TONE_HIGH, 300, 200, 200, 100, /* power off */
	TONE_LOW, 400, 200, 300, 100,  /* send temp */
	TONE_LOW, 400, 200, 300, 100,  /* warning */
};

static void start_buzzer(unsigned char tone)
{
	ther_wake_lock();

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
	/* Setting T4CTL to 0 disables it and masks the overflow interrupt */
	T4CTL = 0;

	P1_BUZZER_PIN = 1;

	/* Return output pin to GPIO */
//	P1SEL &= (uint8) ~HAL_BUZZER_P1_GPIO_PINS;

	ther_wake_unlock();
}

void ther_buzzer_start_music(unsigned char music)
{
	struct ther_buzzer *b = &buzzer;
	unsigned char tone;

	if (b->cur_step != 0)
		return;

	print(LOG_DBG, MODULE "start music %d\n", music);

	b->music = music;
	b->cur_step = STEP_START;
	tone = musics[b->music][TONE_OFFSET];
	start_buzzer(tone);
	osal_start_timerEx(b->task_id, TH_BUZZER_EVT, musics[b->music][b->cur_step]);
}

void ther_buzzer_stop_music(void)
{
	struct ther_buzzer *b = &buzzer;

	if (b->cur_step == 0)
		return;

	print(LOG_DBG, MODULE "stop music %d\n", b->music);

	osal_stop_timerEx(b->task_id, TH_BUZZER_EVT);

	if (b->cur_step % 2)
		stop_buzzer();

	b->cur_step = 0;
}

void ther_buzzer_playing_music(void)
{
	struct ther_buzzer *b = &buzzer;
	unsigned char tone;

	if (b->cur_step == 0) {
		print(LOG_DBG, MODULE "why cur step is %d ??\n", b->cur_step);
	}

	b->cur_step++;

	if (b->cur_step == STEP_END) {
		print(LOG_DBG, MODULE "end music %d\n", b->music);
		stop_buzzer();
		b->cur_step = 0;
		return;
	}

	if (b->cur_step % 2) {
		tone = musics[b->music][TONE_OFFSET];
		start_buzzer(tone);

	} else {
		stop_buzzer();
	}

	osal_start_timerEx(b->task_id, TH_BUZZER_EVT, musics[b->music][b->cur_step]);
}

void ther_buzzer_init(unsigned char task_id)
{
	struct ther_buzzer *b = &buzzer;

	print(LOG_INFO, MODULE "buzzer init\n");

	osal_memset(b, 0, sizeof(struct ther_buzzer));

	b->task_id = task_id;

	P1_BUZZER_PIN = 1;
}

void ther_buzzer_exit(void)
{
	struct ther_buzzer *b = &buzzer;

	print(LOG_INFO, MODULE "buzzer exit\n");

	/*
	 * logic ok?
	 */
	osal_stop_timerEx(b->task_id, TH_BUZZER_EVT);
//	osal_clear_event(b->task_id, TH_BUZZER_EVT);

	if (b->cur_step >= STEP_START && b->cur_step <= STEP_END) {
		if (b->cur_step % 2)
			stop_buzzer();
	}
}

