/*
 * temphumid.c
 *
 * Created: 09/11/2018 15:29:31
 * Author : jncro
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/wdt.h>


#define SLA (0x27 << 1)
#define SLAR (SLA | 0x1)

#define TWINT_WAIT while(!(TWCR & (1<<TWINT)));

#define STATE_PRE_MEASURE	0
#define STATE_PRE_READ		1

#define ms_delay 5
const int pre_read_delay = 100 / ms_delay;
const int pre_measure_delay = 100 / ms_delay;

static int state = STATE_PRE_MEASURE;
static int cnt;

static int32_t temp_value, humid_value;

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

	TWDR = SLA;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x18)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -2;
	}
			
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
		
	// Send address + read bit
	TWDR = SLAR;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	if((TWSR & 0xf8) != 0x40)
	{
		TWCR = (1 << TWINT) | (1 << TWEN);
		return -2;
	}
			
	// Read 4 data bytes
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	uint8_t b0 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	uint8_t b1 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
	TWINT_WAIT;
	uint8_t b2 = TWDR;
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWINT_WAIT;
	uint8_t b3 = TWDR;
		
	// Send stop
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
		
	int status = b0 >> 6;
	if(status == 0)
	{
		// Fresh data available
		
		int32_t temp_val = (((int32_t)b2) << 6) + (((int32_t)b3) >> 2);
		temp_val *= 16500;
		temp_val /= (16384 - 2);
		temp_val -= 4000;
		temp_value = temp_val;	// store to global - this is returned by USB
			
		// temp val is now in degrees C*100
		// convert to a scale between 0 and 50 and output
		if(temp_val < 0)
		temp_val = 0;
		if(temp_val > 5000)
		temp_val = 5000;
		temp_val *= 0x3ff;
		temp_val /= 5000;
			
		int32_t humid_val = (int32_t)b1 + ((int32_t)(b0 & 0x3f) << 8);
		humid_val *= 10000;
		humid_val /= (16384 - 2);
		humid_value = humid_val;  // store to global - this is returned by USB
			
		// humid val is now in % * 100
		humid_val *= 0x3ff;
		humid_val /= 10000;
		
		// output
		OCR1A = temp_val & 0x3ff;
		OCR1B = humid_val & 0x3ff;
			
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
	_delay_ms(500);
	dbg(0);
	
	/* Set TWI baud rate to 100 kHz */
	PRR &= ~(1 << PRTWI);	// enable TWI
	TWSR = 0;	// prescaler = 1
	TWBR = 38;
	
	// Disable pullups on port C
	PORTC = 0;
	
	// Wait for HIH to be ready
	_delay_ms(60);
	
	
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
	while(1)
	{
		// check D4 input
		if(PIND & (1 << PIND4))
		{
			// D4 not pressed - run main loop
			if(cnt-- == 0)
			{
				switch(state)
				{
					case STATE_PRE_MEASURE:
						if(request_measurement() == 0)
						{
							state = STATE_PRE_READ;
							cnt = pre_read_delay;
						}
						else
						{
							state = STATE_PRE_MEASURE;
							cnt = pre_measure_delay;
						}
						break;
						
					case STATE_PRE_READ:
						read_result();
						state = STATE_PRE_MEASURE;
						cnt = pre_read_delay;
						break;	
				}
			}
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
