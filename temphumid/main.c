/*
Copyright (c) 2018  John Cronin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
 * temphumid.c
 *  Firmware for TempHumid USB/Analog out device
 *
 * Created: 09/11/2018 15:29:31
 * Author : jncro
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <usbdrv.h>


#define SLA (0x27 << 1)
#define SLAR (SLA | 0x1)

#define TWINT_WAIT while(!(TWCR & (1<<TWINT)));

#define STATE_PRE_MEASURE	0
#define STATE_PRE_READ		1

#define ms_delay 1
const int pre_read_delay = 100 / ms_delay;
const int pre_measure_delay = 100 / ms_delay;

static int state = STATE_PRE_MEASURE;
static int cnt;

static int32_t usb_val[2];
static int32_t temp_val, humid_val;

PROGMEM char const usbDescriptorHidReport[48] = {
	0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
	0x09, 0x01,                    //   USAGE (Vendor Usage 1)
	0xa1, 0x00,                    // COLLECTION (Physical)
	0x09, 0x01,                    //   USAGE (Vendor Usage 1)
	0x85, 0x01,                    // REPORT_ID (1)
	0x95, 0x01,                    // REPORT_COUNT (1)
	0x75, 0x20,                    // REPORT_SIZE (32)
	0x26, 0x74, 0x40,              // LOGICAL_MAXIMUM (16500)
	0x16, 0x60, 0xf0,              // LOGICAL_MINIMUM (-4000)
	0x35, 0xd8,                    // PHYSICAL_MINIMUM (-40)
	0x46, 0xa5, 0x00,              // PHYSICAL_MAXIMUM (165)
	0x81, 0x02,                    // INPUT (Data,Var,Abs)
	0x09, 0x02,                    //   USAGE (Vendor Usage 2)
	0x85, 0x02,                    // REPORT_ID (2)
	0x95, 0x01,                    // REPORT_COUNT (1)
	0x75, 0x20,                    // REPORT_SIZE (32)
	0x15, 0x00,                    // LOGICAL_MINIMUM (0)
	0x26, 0x10, 0x27,              // LOGICAL_MAXIMUM (10000)
	0x35, 0x00,                    // PHYSICAL_MINIMUM (0)
	0x45, 0x64,                    // PHYSICAL_MAXIMUM (100)
	0x81, 0x02,                    //   INPUT (Data,Var,Abs)
	0xc0                           // END_COLLECTION
};


static void dbg(int val)
{
	if(val & 1)
		PORTD |= (1 << PORTD6);
	else
		PORTD &= ~(1 << PORTD6);
	if(val & 2)
		PORTD |= (1 << PORTD7);
	else
		PORTD &= ~(1 << PORTD7);
	if(val & 4)
		PORTB |= (1 << PORTB0);
	else
		PORTB &= ~(1 << PORTB0);
}

static int request_measurement()
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x08 && (TWSR & 0xf8) != 0x10)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -1;
	}
	
	usbPoll();

	TWDR = SLA;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x18)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -2;
	}
	
	usbPoll();
			
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
	return 0;
}

static int read_result()
{
	// Send start
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x08 && (TWSR & 0xf8) != 0x10)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -1;
	}
	
	usbPoll();
		
	// Send address + read bit
	TWDR = SLAR;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x40)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -2;
	}
	
	usbPoll();
			
	// Read 4 data bytes
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	usbPoll();
	uint8_t b0 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	usbPoll();
	uint8_t b1 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	usbPoll();
	uint8_t b2 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	usbPoll();
	uint8_t b3 = TWDR;
		
	// Send stop
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);

	usbPoll();
		
	int status = b0 >> 6;
	if(status == 0)
	{
		// Fresh data available
		
		temp_val = (((int32_t)b2) << 6) + (((int32_t)b3) >> 2);
		temp_val *= 16500;
		temp_val /= (16384 - 2);
		temp_val -= 4000;
		usb_val[0] = temp_val;	// store to global - this is returned by USB
			
		// temp val is now in degrees C*100
		// convert to a scale between 0 and 50 and output
		if(temp_val < 0)
		temp_val = 0;
		if(temp_val > 5000)
		temp_val = 5000;
		temp_val *= 0x3ff;
		temp_val /= 5000;
		
		usbPoll();
			
		humid_val = (int32_t)b1 + ((int32_t)(b0 & 0x3f) << 8);
		humid_val *= 10000;
		humid_val /= (16384 - 2);
		usb_val[1] = humid_val;  // store to global - this is returned by USB
			
		// humid val is now in % * 100
		humid_val *= 0x3ff;
		humid_val /= 10000;
			
		return 0;
	}
	else if(status == 1)
	{
		// Stale data only - no need to update output
		return 0;
	}
	else
	{
		// Device is in command mode
		return -3;
	}
}

int main(int argc, char const *argv[])
{
	cnt = pre_read_delay;
	
	//wdt_disable();
	wdt_reset();
	wdt_disable();
	
	// Set up debug LEDs
	DDRD |= (1 << DDD6) | (1 << DDD7);
	DDRB |= (1 << DDB0);
	
	dbg(7);
	//while(1);
	//_delay_ms(500);
	dbg(0);
	
	/* Set TWI baud rate to 100 kHz */
	PRR &= ~(1 << PRTWI);	// enable TWI
	TWSR = 0;	// prescaler = 1
	TWBR = 38;
	
	// Disable pullups on port C
	PORTC = 0;
	
	// Wait for HIH to be ready
	//_delay_ms(60);
	
	
	/* Set both PWM outputs to fast PWM mode,
	count from BOTTOM (0) to TOP (0x3ff),
	set the output pin on BOTTOM,
	clear it on OCR match.
	Use internal clock source without prescaler. */
	
	TCCR1A = (2 << COM1A0) | (2 << COM1B0) | 3;
	TCCR1B = (1 << WGM12) | (1 << CS10);
	
	// Set 50 % duty cycle
	OCR1A = 0x1ff;
	OCR1B = 0x1ff;
	
	// Set B1 (OC1A) and B2 (OC1B) as output
	DDRB |= (1 << DDB1) | (1 << DDB2);
	
	// Set D4 as input with pullup active (this is the switch which
	//  triggers the square wave)
	DDRD &= ~(1 << DDD4);
	PORTD |= (1 << PORTD4);
	
	// cnt2 continuously increments to time the square wave output
	int cnt2 = 0;
	
	// Initialize USB and fake a device disconnect/reconnect to ensure the
	//  host gives us a valid USB ID
	DDRD &= ~(1 << DDD2);
	DDRD &= ~(1 << DDD3);
	PORTD &= ~(1 << PORTD2);
	PORTD &= ~(1 << PORTD3);
	usbInit();
	usbDeviceDisconnect();
	_delay_ms(250);
	usbDeviceConnect();
	
	sei();
	
#ifdef DEBUG
	dbg(7);
#endif
	
	while(1)
	{
		usbPoll();
		
		/* State machine to handle I2C comms */
		if(cnt-- == 0)
		{
			switch(state)
			{
				case STATE_PRE_MEASURE:
				if(request_measurement() == 0)
				{
					state = STATE_PRE_READ;
					cnt = pre_read_delay;
#ifdef DEBUG
					dbg(1);
#endif
				}
				else
				{
					state = STATE_PRE_MEASURE;
					cnt = pre_measure_delay;
					
#ifdef DEBUG
					dbg(4);
#endif
				}
				break;
				
				case STATE_PRE_READ:
				read_result();
				state = STATE_PRE_MEASURE;
				cnt = pre_read_delay;
#ifdef DEBUG
				dbg(2);
#endif
				break;
			}
		}

		// check D4 input
		if(PIND & (1 << PIND4))
		{
			// D4 not pressed - output actual values
			OCR1A = temp_val & 0x3ff;
			OCR1B = humid_val & 0x3ff;
		}
		else if(cnt2 & 0x100)	// output square wave low part
		{
			OCR1A = 0;
			OCR1B = 0;
		}
		else					// output square wave high part
		{
			OCR1A = 0x3ff;
			OCR1B = 0x3ff;
		}
		cnt2++;
		
		_delay_ms(ms_delay);
	}
	
}

// HID report return
uchar hidRpt[5];

static uchar* makeReport(uchar id, int32_t val)
{
	dbg(7);
	hidRpt[0] = id;
	hidRpt[1] = (uchar)(val & 0xff);
	hidRpt[2] = (uchar)((val >> 8) & 0xff);
	hidRpt[3] = (uchar)((val > 16) & 0xff);
	hidRpt[4] = (uchar)((val >> 24) & 0xff);
	return hidRpt;
}

// USB Custom device function only using the default endpoint
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (usbRequest_t *)data;
	uchar rqtype = rq->bmRequestType & USBRQ_TYPE_MASK;
	
	if(rqtype == USBRQ_TYPE_VENDOR)
	{
		if(rq->bRequest == 1)
		{
			usbMsgPtr = (uchar*)&usb_val[0];
			return 8;
		}
	}
	else if(rqtype == USBRQ_TYPE_CLASS)
	{
		if(rq->bRequest == USBRQ_HID_GET_REPORT)
		{
			uchar rptid = rq->wValue.bytes[0];
			if(rptid == 1)
			{
				dbg(7);
				usbMsgPtr = (uchar*)&usb_val[0];
				return 4;
			}
			else if(rptid == 2)
			{
				dbg(7);
				usbMsgPtr = (uchar*)&usb_val[1];
				return 4;
			}
		}
	}
	return 0;
}