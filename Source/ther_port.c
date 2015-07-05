/*
 * THER PORT
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

#define MODULE "[THER PORT] "

static unsigned char boost_cnt = 0;

void power_on_boost(void)
{
	boost_cnt++;
	P1_BOOST_EN_PIN = 1;
}

void power_off_boost(void)
{
	boost_cnt--;
	if (boost_cnt == 0)
		P1_BOOST_EN_PIN = 0;
}

void ther_port_init(void)
{
	/*
	 * P0
	 *     P0.0  AIN0
	 *     P0.1  AIN1
	 *     P0.2  UART RX: peripheral mode, alt 1
	 *     P0.3  UART TX: peripheral mode, alt 1
	 *     P0.4  HW VERSION: gpio, output
	 *     P0.5  HW VERSION: gpio, output
	 *     P0.6  TP: gpio, output
	 *     P0.7  ADC Vref: peripheral mode
	 *
	 * P1
	 *     P1.0  BUZZER: peripheral mode, pwm, timer4,
	 *     P1.1  SPI-VCC: gpio, output  -> Not used
	 *     P1.2  BOOST-EN(spi-vcc/oled-vcc, 3.3v): gpio, output
	 *     P1.3  BUTTON: gpio, input, 0 default, rising edge interrupt
	 *     P1.4  SPI-CS: gpio, ouput
	 *     P1.5  SPI-SCK: peripheral mode, USART1 SPI alt.2
	 *     P1.6  SPI-MOSI: peripheral mode, USART1 SPI alt.2
	 *     P1.7  SPI-MISO: peripheral mode, USART1 SPI alt.2
	 *
	 * P2
	 *     P2.0  OLED-VDD-EN: gpio output
	 *     P2.1  DBG-DATA
	 *     P2.2  DBG-CLK
	 *
	 *     P2.3  LDO-EN(for ADC Vref, 2V): gpio, output	-> disable 32K XOSC first
	 *     P2.4  SPI-WP: gpio output					-> disable 32K XOSC first
	 *           P2.3 and P2.4 is mulplex with XOSC32K, so if we want to use it as gpio,
	 *           we need set OSC32K_INSTALLED=FALSE in IAR options.
	 *           see datasheet <7.8 32-kHz XOSC Input>
	 *
	 * PxSEL: 0-gpio 1-peripheral			x = {0 | 1}
	 * PxDIR: 0-input 1-ouput				x = {0 | 1}
	 * PxINP: 0-pullup/pulldown 1-3state	x = {0 | 1}
	 *
	 * APCFG: 0-adc disable, 1-adc enable => override P0SEL
	 *
	 * PICTL: 0-rising edge, 1-failing edge
	 * PxIEN: 0-interrupt disable, 1-interrupt enable
	 * PxIFG: 0-no interrupt pending, 1-interrupt request pending
	 */

	P0SEL = BV(P0_UART_RX_BIT) | BV(P0_UART_TX_BIT);
	P0DIR = BV(P0_HW1_BIT) | BV(P0_HW2_BIT) | BV(P0_TP_BIT);
	P0INP = 0;
	APCFG = BV(P0_AIN0_BIT) | BV(P0_AIN1_BIT) | BV(P0_ADCREF_BIT);

	P1SEL = BV(P1_BUZZER_BIT) | BV(P1_SPI_SCK_BIT) | BV(P1_SPI_MOSI_BIT) | BV(P1_SPI_MISO_BIT);
	P1DIR = BV(P1_SPI_VCC_BIT) | BV(P1_BOOST_EN_BIT) | BV(P1_SPI_CS_BIT);
	P1INP = BV(P1_BUTTON_BIT);

	IEN2 |= BV(4); /* P1 interrupt enable, use |= instead of = */
	P1IEN = BV(P1_BUTTON_BIT); /* enable interrupt */
	P1IFG = 0; /* clear the pending flag */

	PERCFG = BV(1); /* USART1 SPI alt.2(U1CFG) */

	P2SEL = 0;
	P2DIR = BV(P2_OLED_VDDEN_BIT) | BV(P2_LDO_EN_BIT) | BV(P2_SPI_WP_BIT);
	P2INP = 0;

	P0 = 0;
	P1 = 0;
	P2 = 0;
}

void ther_port_exit(void)
{
	boost_cnt = 0;
	P1_BOOST_EN_PIN = 0;
}
