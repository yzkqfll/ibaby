
#ifndef __THER_PORT_H__
#define __THER_PORT_H__

/*
 * Pin bit definition
 */
#define P0_AIN0_BIT 0
#define P0_AIN1_BIT 1
#define P0_UART_RX_BIT 2
#define P0_UART_TX_BIT 3
#define P0_HW1_BIT 4
#define P0_HW2_BIT 5
#define P0_LDO_EN_BIT 6
#define P0_ADCREF_BIT 7

#define P1_BUZZER_BIT 0
#define P1_SPI_VCC_BIT 1 /* NOT used yet */
#define P1_BOOST_EN_BIT 2
#define P1_BUTTON_BIT 3
#define P1_SPI_CS_BIT 4
#define P1_SPI_SCK_BIT 5
#define P1_SPI_MOSI_BIT 6
#define P1_SPI_MISO_BIT 7

#define P2_OLED_VDDEN_BIT 0
#define P2_DBG_DATA_BIT 1
#define P2_DBG_CLK_BIT 2
#define P2_XOSC_32K_Q2_BIT 3
#define P2_XOSC_32K_Q1_BIT 4

/*
 * Pin value
 */
#define P0_AIN0_PIN P0_0
#define P0_AIN1_PIN P0_1
#define P0_UART_RX_PIN P0_2
#define P0_UART_TX_PIN P0_3
#define P0_HW1_PIN P0_4
#define P0_HW2_PIN P0_5
#define P0_LDO_EN_PIN P0_6
#define P0_ADCREF_PIN P0_7

#define P1_BUZZER_PIN P1_0
#define P1_SPI_VCC_PIN P1_1
#define P1_BOOST_EN_PIN P1_2
#define P1_BUTTON_PIN P1_3
#define P1_SPI_CS_PIN P1_4
#define P1_SPI_SCK_PIN P1_5
#define P1_SPI_MOSI_PIN P1_6
#define P1_SPI_MISO_PIN P1_7

#define P2_OLED_VDDEN_PIN P2_0
#define P2_DBG_DATA_PIN P2_1
#define P2_DBG_CLK_PIN P2_2
#define P2_LDO_EN_PIN P2_3
#define P2_SPI_WP_PIN P2_4

void ther_port_init(void);
void ther_port_exit(void);

#endif

