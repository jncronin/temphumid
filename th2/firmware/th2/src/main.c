/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to system_init()
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include <wdt.h>

#define STATUSR PORT_PA05
#define STATUSG PORT_PA04
#define SWITCH	PORT_PA16

#define I2C_ADDR_HIH	(0x27 << 1)
#define I2C_ADDR_TDAC	(0x60 << 1)
#define I2C_ADDR_HDAC	(0x61 << 1)

// interval between updates in multiples of 50ms
#define POLL_DELAY		10

// interval between calibration high and low in multiples of 50ms
#define CALIB_DELAY		20

volatile int state = 0;
volatile int bytes_read = 0;
volatile uint8_t hih_val[4];
volatile uint32_t usb_val[2];
volatile uint32_t cur_tick = 0;
volatile uint32_t hih_request_at = 0;
volatile uint32_t tdac_val = 0;
volatile uint32_t hdac_val = 0;

volatile bool cdc_enabled = false;

#define STATE_IDLE			0
#define STATE_HIH_REQUEST	1
#define STATE_HIH_RECEIVE	2
#define STATE_DAC0_WRITE	3
#define STATE_DAC0_WRITE2	4
#define STATE_DAC0_WRITE3	5
#define STATE_DAC1_WRITE	6
#define STATE_DAC1_WRITE2	7
#define STATE_DAC1_WRITE3	8


static void request_measurement(void);

// USB output
/*static void puthex(uint32_t v)
{
	for(int i = 7; i >= 0; i--)
	{
		uint32_t v2 = v >> (i * 4);
		v2 &= 0xf;
		
		if(v2 < 0xa)
			udi_cdc_putc('0' + v2);
		else
			udi_cdc_putc('a' + v2 - 0xa);
	}
}*/

static void usb_output(void)
{
	/* if(cdc_enabled == false)
		return;
		
	udi_cdc_putc('T');
	puthex(usb_val[0]);
	udi_cdc_putc('H');
	puthex(usb_val[1]); */
}

// 50ms tick
void TC1_Handler()
{
	static int poll_cnt = 0;
	cur_tick++;
	poll_cnt++;
		
	if((PORT->Group[0].IN.reg & SWITCH) == 0)
	{
		if((cur_tick / CALIB_DELAY) & 0x1)
		{
			tdac_val = 0xfff;
			hdac_val = 0xfff;
		}
		else
		{
			tdac_val = 0;
			hdac_val = 0;
		}
	}
	
	if(cur_tick == hih_request_at)
	{
		// we need to request fresh data from the hih
		// first, set ACKACT to 0 so we automatically acknowledge on reading DATA
		SERCOM0->I2CM.CTRLB.bit.ACKACT = 0;
		while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				
		// write to address register
		state = STATE_HIH_RECEIVE;
		bytes_read = 0;
		SERCOM0->I2CM.ADDR.reg = I2C_ADDR_HIH | 0x1;
	}
	
	if(poll_cnt >= POLL_DELAY)
	{
		request_measurement();
		poll_cnt = 0;
	}
	
	//PORT->Group[0].OUTTGL.reg = STATUSR | STATUSG;
	TC1->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
}

void SERCOM0_Handler()
{
	while(SERCOM0->I2CM.INTFLAG.bit.MB)
	{
		switch(state)
		{
			case STATE_HIH_REQUEST:
				// send stop condition
				SERCOM0->I2CM.CTRLB.bit.CMD = 0x3;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
			
				// now request result in 100 ms time
				hih_request_at = cur_tick + 2;
				break;
				
			case STATE_DAC0_WRITE:
				// send first data byte
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 0;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				state = STATE_DAC0_WRITE2;
				SERCOM0->I2CM.DATA.reg = tdac_val >> 8;
				break;
				
			case STATE_DAC0_WRITE2:
				// send second data byte
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 1;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				state = STATE_DAC0_WRITE3;
				SERCOM0->I2CM.DATA.reg = tdac_val & 0xff;
				break;
				
			case STATE_DAC0_WRITE3:
				// send stop byte
				SERCOM0->I2CM.CTRLB.bit.CMD = 3;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				
				// send dac1 address
				state = STATE_DAC1_WRITE;
				SERCOM0->I2CM.ADDR.reg = I2C_ADDR_HDAC | 0x0;
				break;

			case STATE_DAC1_WRITE:
				// send first data byte
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 0;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				state = STATE_DAC1_WRITE2;
				SERCOM0->I2CM.DATA.reg = hdac_val >> 8;
				break;
			
			case STATE_DAC1_WRITE2:
				// send second data byte
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 1;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				state = STATE_DAC1_WRITE3;
				SERCOM0->I2CM.DATA.reg = hdac_val & 0xff;
				break;
							
			case STATE_DAC1_WRITE3:
				// send stop byte
				SERCOM0->I2CM.CTRLB.bit.CMD = 3;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				
				// end 
				state = STATE_IDLE;
				break;				

			default:
				// mb set unexpectedly
				PORT->Group[0].OUTSET.reg = STATUSR;
				PORT->Group[0].OUTCLR.reg = STATUSG;
				SERCOM0->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_MB;
		}
	}
	while(SERCOM0->I2CM.INTFLAG.bit.SB)
	{
		if(state == STATE_HIH_RECEIVE)
		{
			if(bytes_read <= 2)
			{
				// acknowledge data byte and request more
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 0;
			}
			else
			{
				// nack next byte
				SERCOM0->I2CM.CTRLB.bit.ACKACT = 1;
			}
			while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				
			//cpu_irq_disable();
			hih_val[bytes_read++] = SERCOM0->I2CM.DATA.reg;
			
			if(bytes_read == 4)
			{
				state++;
				bytes_read = 0;
				
				// send stop condition
				SERCOM0->I2CM.CTRLB.bit.CMD = 0x3;
				while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
				
				int status = hih_val[0] >> 6;
				
				if(status == 0)
				{
					uint32_t humid_val = (uint32_t)hih_val[1] + (((uint32_t)hih_val[0] & 0x3f) << 8);
					uint32_t temp_val = ((uint32_t)hih_val[3] >> 2) + ((uint32_t)hih_val[2] << 6);
				
					temp_val *= 16500;
					temp_val /= (16384 - 2);
					temp_val -= 4000;

					humid_val *= 10000;
					humid_val /= (16384 - 2);
				
					usb_val[0] = temp_val;
					usb_val[1] = humid_val;
					
					if((PORT->Group[0].IN.reg & SWITCH) != 0)
					{
						/* Scale temp to 0-50 and humid to 0-100 within a 12 bit DAC space */
						tdac_val = temp_val * 0xfff / 5000;
						hdac_val = humid_val * 0xfff / 10000;
					}
				}
			}
			//cpu_irq_enable();
			
			if(!bytes_read)
			{
				usb_output();
				PORT->Group[0].OUTSET.reg = STATUSG;
				
				// begin writing the tdac value
				state = STATE_DAC0_WRITE;
				SERCOM0->I2CM.ADDR.reg = I2C_ADDR_TDAC | 0x0;
			}
		}
		else
		{
			// sb set unexpectedly
			PORT->Group[0].OUTSET.reg = STATUSG | STATUSR;
			SERCOM0->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_SB;
		}
	}
	//SERCOM0->I2CM.INTFLAG.reg = SERCOM_I2CM_INTFLAG_MB | SERCOM_I2CM_INTFLAG_SB;
}

static void request_measurement(void)
{
	// requesting a measurement is a write transaction with no data
	state = STATE_HIH_REQUEST;
	SERCOM0->I2CM.ADDR.reg = I2C_ADDR_HIH | 0x0;
		
	// further handling will be done by ISR
}

int main (void)
{
	system_init();

	/* Insert application code here, after the board has been initialized. */
	
	// test status green led
	PORT->Group[0].DIRSET.reg = STATUSR | STATUSG;
	PORT->Group[0].OUTSET.reg = STATUSG;
	
	// configure switch as input with pullup active
	PORT->Group[0].DIRCLR.reg = SWITCH;
	//PORT->Group[0].WRCONFIG.reg = PORT_WRCONFIG_HWSEL | PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_PULLEN |
	//	PORT_WRCONFIG_INEN | PORT_WRCONFIG_PINMASK(0);
	PORT->Group[0].PINCFG[16].reg = PORT_PINCFG_INEN | PORT_PINCFG_PULLEN;
	PORT->Group[0].OUTSET.reg = SWITCH;
	
	// tc1 at 50 ms intervals
	TC1->COUNT16.CTRLA.reg = 0;
	PM->APBCMASK.reg |= PM_APBCMASK_TC1;
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_TC1_TC2;
	TC1->COUNT16.CTRLA.reg = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ |
		TC_CTRLA_PRESCALER_DIV1;
	TC1->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
	
	TC1->COUNT16.CC[0].reg = 50;
	TC1->COUNT16.INTENSET.reg = TC_INTENSET_MC0;
	
	// set up sercom0 for i2c on pa06 and pa07
	PORT->Group[0].WRCONFIG.reg = PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_WRPMUX |
		PORT_WRCONFIG_PMUXEN | PORT_WRCONFIG_PMUX(PORT_PMUX_PMUXE_C) |
		PORT_WRCONFIG_PINMASK(PORT_PA06 | PORT_PA07);
		
	PM->APBCMASK.reg |= PM_APBCMASK_SERCOM0;
	
	/* clock calc for i2c: (aim for 100 kHz)
		for normal mode, worse-case Trise is 350 ns
		
		from datasheet, Fscl = Fgclk / (2 * (5 + BAUD) + Fgclk * Trise)
		or
		BAUD = Fgclk / 2 * (1/Fscl - Trise) - 5
		
		Assume GCLK0 ie 48Mhz
		
		BAUD = 24000000 * (1/100000 - 350*10^-9) - 5
		     = 226 */	
	
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_SERCOM0_CORE;
	SERCOM0->I2CM.CTRLA.reg = SERCOM_I2CM_CTRLA_SPEED(0) |
			SERCOM_I2CM_CTRLA_SDAHOLD(2) |						// 450ns hold SDA after SCL change (DAC requires at least 100ns)
			SERCOM_I2CM_CTRLA_MODE_I2C_MASTER;
	SERCOM0->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;			// smart mode
	while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
	SERCOM0->I2CM.BAUD.reg = 226;
	while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
	SERCOM0->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
	while(SERCOM0->I2CM.SYNCBUSY.reg & SERCOM_I2CM_SYNCBUSY_ENABLE);
	SERCOM0->I2CM.STATUS.bit.BUSSTATE = 0x1;					// idle state
	while(SERCOM0->I2CM.SYNCBUSY.bit.SYSOP);					// sync wait
	SERCOM0->I2CM.INTENSET.reg = SERCOM_I2CM_INTENSET_MB | SERCOM_I2CM_INTENSET_SB;		// interrupt on write and read complete	
	
	udc_start();
	
	struct wdt_conf wc;
	wdt_get_config_defaults(&wc);
	wc.always_on = false;
	wc.enable = false;
	wdt_set_config(&wc);
	
	// Enable interrupts	
	system_interrupt_enable(SYSTEM_INTERRUPT_MODULE_TC1);
	system_interrupt_enable(SERCOM0_IRQn);
	cpu_irq_enable();
	
	// read request from the hih
	//request_measurement();
	
	
	while(1);
}

// usb functions
/*bool my_usb_enable(uint8_t port)
{
	(void)port;
	cdc_enabled = true;
	return true;
}

void my_usb_disable(uint8_t port)
{
	(void)port;
	cdc_enabled = false;
}*/

/*void udi_cdc_recv(uint8_t port)
{
	(void)port;
	while(udi_cdc_get_nb_received_data())
	{
		udi_cdc_getc();
	}
}*/

/*extern udd_ctrl_request_t udd_g_ctrlreq;
bool udi_hid_setup( uint8_t *rate, uint8_t *protocol, uint8_t *report_desc, bool (*setup_report)(void) )
{
	udd_ctrl_request_t* cr = &udd_g_ctrlreq;
	
	if(udd_g_ctrlreq.req.bmRequestType
}*/

bool parse_usb_request(void)
{
	uint32_t wv = udd_g_ctrlreq.req.wValue;
	
	if((wv & 0xff00) == (1 << 8))
	{
		switch(udd_g_ctrlreq.req.wIndex)
		{
			case 1:
				udd_g_ctrlreq.payload = (uint8_t *)&usb_val[0];
				udd_g_ctrlreq.payload_size = 4;
				return true;
			case 2:
				udd_g_ctrlreq.payload = (uint8_t *)&usb_val[1];
				udd_g_ctrlreq.payload_size = 4;
				return true;
			default:
				udd_g_ctrlreq.payload = (uint8_t *)&usb_val[0];
				udd_g_ctrlreq.payload_size = 8;
				return true;
		}
	}
	return false;
}