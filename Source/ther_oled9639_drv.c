
#include "osal.h"

#include "hal_board.h"
#include "hal_i2c.h"
#include "ther_uart_comm.h"

#include "ther_oled9639_drv.h"

#define MODULE "[OLED DRV] "

/*
 * P1.2 : bootst-en pin
 */
#define BOOST_EN_PIN_VAL P1_2
#define BOOST_EN_PIN 2

/*
 * P2.0 : VDD enable
 */
#define VDD_EN_PIN_VAL P2_0
#define VDD_EN_PIN 0

#define OLED_IIC_ADDR 0x3C

#define BUF_LEN 2

#define TYPE_CMD 0x0
#define TYPE_DATA 0x40

/*
 * 1. Fundamental Command Table
 */
#define CMD_CONTRAST 0x81

enum {
	NORMAL_DISPLAY = 0,
	ENTIRE_DISPLAY_ON,
};
#define CMD_ENTIRE_DISPLAY(x) (0xA4 + (x))

enum {
	INVERSE_OFF = 0, /* normal display */
	INVERSE_ON,
};
#define CMD_DISPLAY_INVERSE(x) (0xA6 + (x))

enum {
	DISPLAY_OFF = 0,
	DISPLAY_ON,
};
#define CMD_DISPLAY_ONOFF(x) (0xAE + (x))

/*
 * 2. Scrolling Command Table
 */

/*
 * 3. Addressing Setting Command Table
 */
#define CMD_START_COL_LOW(x) (0x0 + (x) % 16)
#define CMD_START_COL_HIGH(x) (0x10 + (x) / 16)

#define CMD_ADDRESSING_MODE 0x20
enum {
	HORIZONTAL_ADDRESSING_MODE = 0,
	VERTICAL_ADDRESSING_MODE,
	PAGE_ADDRESSING_MODE,
};


#define CMD_START_PAGE(x) (0xB0 + (x))

/*
 * 4. Hardware Configuration (Panel resolution & layout related) Command Table
 */

#define CMD_DISP_START_LINE(x) (0x40 + (x)) /* x: [0, 38]*/

#define CMD_MULTIPLEX_RATIO 0xA8

enum {
	COM_REMAP_DISABLE = 0,
	COM_REMAP_ENABLE,
};
#define CMD_COM_REMAP(x) (0xC0 + (x) << 3)

#define CMD_DISPLAY_OFFSET 0xD3

enum {
	REMAP_OFF = 0,
	REMAP_ON,
};
#define CMD_SEGMENT_REMAP(x) (0xA0 + (x))


#define CMD_COM_CONFIG 0xDA
enum {
	SEQ_COM_CONFIG = 0,
	ALTERNATIVE_COM_CONFIG
};
enum {
	DISABLE_COM_REMAP = 0,
	ENABLE_COM_REMAP,
};
#define COM_CONFIG(pin_cfg, remap) ((pin_cfg) << 4 | (remap << 5) | 0x2)

/*
 * 5. Timing & Driving Scheme Setting Command Table
 */
#define CMD_DISP_CLK_DIV 0xD5

#define CMD_PRECHARGE_PERIOD 0xD9

enum {
	VCOMH_LEVEL_LOW = 0x00, /* ~ 0.65 x VCC */
	VCOMH_LEVEL_MID = 0x20, /* ~ 0.77 x VCC (RESET) */
	VCOMH_LEVEL_HIGH = 0x30, /* ~ 0.83 x VCC */
};
#define CMD_VCOMH_DESELECT 0xDB

/*
 * Command Table for Charge Bump Setting
 */
enum {
	CHARGE_PUMP_DISABLE = 0,
	CHARGE_PUMP_ENABLE,
};
#define CMD_CHARGE_PUMP 0x8D
	#define SET_CHARGE_PUMP(x) ((1 << 4) | (x) << 2)


/*
 * Send command to OLED
 * slave addr + type + cmd
 */
static void send_cmd(unsigned char cmd)
{
	unsigned char cnt;
	unsigned char buf[BUF_LEN] = {TYPE_CMD, cmd};

	cnt = HalI2CWrite(BUF_LEN, buf);
	if (cnt != 2) {
		print(LOG_DBG, MODULE "cmd: cnt %d, buf: 0x%x 0x%x\r\n", cnt, buf[0], buf[1]);
	}
}

/*
 * Send data to OLED
 * slave addr + type + data
 */
static void send_data(unsigned char data)
{
	unsigned char cnt;
	unsigned char buf[BUF_LEN] = {TYPE_DATA, data};

	cnt = HalI2CWrite(BUF_LEN, buf);

	if (cnt != 2) {
		print(LOG_DBG, MODULE "data: cnt %d, buf: 0x%x 0x%x\r\n", cnt, buf[0], buf[1]);
	}
}

void uDelay(unsigned char l)
{
	while(l--);
}

void Delay(unsigned char n)
{
unsigned char i,j,k;

	for(k=0;k<n;k++)
	{
		for(i=0;i<131;i++)
		{
			for(j=0;j<15;j++)
			{
				uDelay(203);
			}
		}
	}
}

/*
 * 1. Fundamental Command
 */


/* Set SEG Output Current */
static void set_contrast(unsigned char steps)
{
	send_cmd(CMD_CONTRAST);

	/* 0 ~ 0x7f */
	send_cmd(steps);
}

static void set_entire_display(unsigned char val)
{
	send_cmd(CMD_ENTIRE_DISPLAY(val));
}

static void set_display_inverse(unsigned char val)
{
	send_cmd(CMD_DISPLAY_INVERSE(val));
}


static void set_display_onoff(unsigned val)
{
	send_cmd(CMD_DISPLAY_ONOFF(val));
}

/*
 * 2. Scrolling Command
 */


/*
 * 3. Addressing Setting Command
 */


/*
 * Set Page Start Address for Page Addressing Mode
 */
static void set_start_page(unsigned char start_page)
{
	/*
	 * OLED 96x39 has 39 lines, 8 line per page.
	 * 39 lines => 5 page
	 *
	 * valid range [0, 4]
	 */

	send_cmd(CMD_START_PAGE(start_page));
}

static void set_start_column(unsigned char start_column)
{
	/*
	 * 96 columns
	 */

	send_cmd(CMD_START_COL_LOW(start_column));
	send_cmd(CMD_START_COL_HIGH(start_column));
}

static void set_addressing_mode(unsigned char mode)
{
	send_cmd(CMD_ADDRESSING_MODE);
	send_cmd(mode);
}

/*
 * 4. Hardware Configuration (Panel resolution & layout related) Command
 */

/*
 * Set Mapping RAM Display Start Line (0x00~0x27)
 */
static void set_start_line(unsigned char line)
{
	send_cmd(CMD_DISP_START_LINE(line));
}

/* Set SEG/Column Mapping */
static void set_segment_remap(unsigned char val)
{
	send_cmd(CMD_SEGMENT_REMAP(val));
}

/* 1/39 Duty (0x00~0x27) */
static void set_multiplex_ratio(unsigned char ratio)
{
	send_cmd(CMD_MULTIPLEX_RATIO);
	send_cmd(ratio);
}

/* Set COM/Row Scan Direction */
static void set_com_remap(unsigned char dir)
{
	send_cmd(CMD_COM_REMAP(dir));
}

/* Set Alternative Configuration */
static void set_com_config(unsigned char val)
{
	send_cmd(CMD_COM_CONFIG);
	send_cmd(val);
}

/*
 * Shift Mapping RAM Counter (0x00~0x27)
 */
static void set_display_offset(unsigned char val)
{
	send_cmd(CMD_DISPLAY_OFFSET);
	send_cmd(val);
}

/*
 * 5. Timing & Driving Scheme Setting Command
 */


/* Set Clock as 100 Frames/Sec */
static void set_display_clock(unsigned char clk_div)
{
	send_cmd(CMD_DISP_CLK_DIV);

	/*
	 * D[3:0] => Display Clock Divider
	 * D[7:4] => Oscillator Frequency
	 */
	send_cmd(clk_div);
}

/* Set Pre-Charge as 13 Clocks & Discharge as 2 Clock */
static void set_precharge_period(unsigned char val)
{
	send_cmd(CMD_PRECHARGE_PERIOD);

	/*
	 * val[3:0] : Phase 1 period of up to 15 DCLK
	 *
	 * val[7:4] : Phase 2 period of up to 15 DCLK
	 */
	send_cmd(val);
}

/* Set VCOM Deselect Level */
static void set_vcomh_deselect(unsigned val)
{
	send_cmd(CMD_VCOMH_DESELECT);

	/* A[6:4] */
	send_cmd(val);
}

/*
 * MISC
 */

/* Enable Embedded DC/DC Converter (0x00/0x04) */
static void set_charge_pump(unsigned char val)
{
	send_cmd(CMD_CHARGE_PUMP);
	send_cmd(SET_CHARGE_PUMP(val));
}

enum {
	VCC_POWER_OFF = 0,
	VCC_POWER_ON
};

/*
 * OLED Panel power
 */
static void set_vcc_power(unsigned char val)
{
	BOOST_EN_PIN_VAL = val;
}

enum {
	VDD_POWER_OFF = 0,
	VDD_POWER_ON,
};
/*
 * OLED logic power
 */
static void set_vdd_power(unsigned char val)
{
	VDD_EN_PIN_VAL = val;
}

void oled_drv_display_on(void)
{
	set_display_onoff(DISPLAY_ON);
}

void oled_drv_display_off(void)
{
	set_display_onoff(DISPLAY_OFF);
}

void oled_drv_power_on(void)
{
	set_vcc_power(VCC_POWER_ON);
	set_vdd_power(VDD_POWER_ON);
}

void oled_drv_power_off(void)
{
	set_vcc_power(VCC_POWER_OFF);
	set_vdd_power(VDD_POWER_OFF);
}

void oled_drv_fill_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, unsigned char data)
{
	unsigned char page;
	unsigned char col;

	for (page = start_page; page < end_page; page++) {

		set_start_page(page);

		col = start_col;
		set_start_column(col);
		for (; col < end_col; col++) {
			send_data(data);
		}
	}
}

void oled_drv_write_block(unsigned char start_page, unsigned char end_page,
		unsigned char start_col, unsigned char end_col, unsigned char *data)
{
	unsigned char page;
	unsigned char col;

	for (page = start_page; page < end_page; page++) {

		set_start_page(page);

		col = start_col;
		set_start_column(col);
		for (; col < end_col; col++) {
			send_data(*data++);
		}
	}
}

void oled_drv_fill_screen(unsigned char val)
{
	unsigned char page;
	unsigned char col;

	for (page = 0; page < MAX_PAGE; page++) {

		set_start_page(page);

		col = 0;
		set_start_column(col);
		for (; col < MAX_COL; col++) {
			send_data(val);
		}
	}
}

static void init_gpio(void)
{
	/*
	 * VBOOST enable pin setup
	 * p1.2
	 */
	P1SEL &= ~(1 << BOOST_EN_PIN);
	P1DIR |= 1 << BOOST_EN_PIN;

	/*
	 * VDD enable pin setup
	 * p2.0
	 */
	P2SEL &= ~BV(VDD_EN_PIN);
	P2DIR |= BV(VDD_EN_PIN);
}

void oled_drv_init_device(void)
{
	HalI2CInit(OLED_IIC_ADDR, i2cClock_533KHZ);

	set_display_onoff(DISPLAY_OFF);

	set_start_page(0);
	set_start_column(0);

	set_display_clock(0xA0); // yuanjie: 0x80
	set_multiplex_ratio(0x27); // yuanjie: 0x3f

	set_addressing_mode(PAGE_ADDRESSING_MODE);

	set_display_offset(0);
	set_start_line(0);

	set_segment_remap(REMAP_OFF); /* yuanjie: REMAP_ON */

	set_com_remap(COM_REMAP_ENABLE); /* yuanjie: COM_REMAP_ENABLE */
	set_com_config(COM_CONFIG(ALTERNATIVE_COM_CONFIG, DISABLE_COM_REMAP));

	set_contrast(0xCF);

	set_charge_pump(CHARGE_PUMP_ENABLE);
	set_precharge_period(0xD2); // yuanjie: 0xf1
	set_vcomh_deselect(VCOMH_LEVEL_HIGH);

	set_entire_display(NORMAL_DISPLAY);
	set_display_inverse(INVERSE_OFF);

	oled_drv_fill_screen(0x0);
}

void oled_drv_init(void)
{
	init_gpio();

	oled_drv_power_on();

	oled_drv_init_device();

}
