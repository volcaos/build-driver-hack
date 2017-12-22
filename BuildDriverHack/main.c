/*
 * main.c
 *
 * Created: 2017/12/22 22:22:22
 * Author : T.Yamaguchi
 */ 

#define F_CPU 16000000L
#define BAUD 9600

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define sbi(PORT,BIT) PORT|=_BV(BIT)
#define cbi(PORT,BIT) PORT&=~_BV(BIT)

enum IR_PHASE
{
	LEADER,
	CUSTOMER_CODE,
	DATA,
	DATA_,
};

volatile uint32_t tc;
volatile uint16_t code_buf;

void enable_external_clock()
{
	CLKSEL0 |= _BV(EXTE) | _BV(CLKS);
	CLKPR = _BV(CLKPCE);
	CLKPR = 0;
}

void num_key( uint8_t num )
{
	code_buf %= 100;
	code_buf *= 10;
	code_buf += num;
}

void set_bottle( uint8_t side )
{
	uint8_t code = code_buf - 1;
	uint8_t d0 = 0;
	uint8_t d1 = 0;
	for( int i=0; i<4; i++ ){
		uint8_t c = code % 3 + 1;
		d0 <<= 2;
		d1 <<= 2;
		d0 |= c&1;
		d1 |= c>>1&1;
		code %= 3;
	}
	// 
	uint8_t clk = 0b10101010;
	uint8_t p0, p1, p2;
	uint8_t pin0, pin1, pin2;
	if( code ){
		p0 = clk; p1 = d0; p2 = d1;
	}else{
		p0 = d0; p1 = d1; p2 = clk;
	}
	if( side ){
		pin0 = PORTB1, pin1 = PORTB2, pin2 = PORTB3;
	}else{
		pin0 = PORTB4, pin1 = PORTB5, pin2 = PORTB6;
	}
	// 
	for( int i=0; i<8; i++ ){
		if( bit_is_set(p0,i) ){
			sbi(PORTB,pin0);
		}else{
			cbi(PORTB,pin0);
		}
		if( bit_is_set(p1,i) ){
			sbi(PORTB,pin1);
		}else{
			cbi(PORTB,pin1);
		}
		if( bit_is_set(p2,i) ){
			sbi(PORTB,pin2);
		}else{
			cbi(PORTB,pin2);
		}
		// 
		_delay_ms(20);
	}
}

void ir_command( uint8_t cmd )
{
	switch(cmd){
		case 0b11111011: set_bottle(0); break;	// |<<
		case 0b11111001: set_bottle(1); break;	// >>|
		case 0b11110011: num_key(0); break;	// 0
		case 0b11101111: num_key(1); break;	// 1
		case 0b11101110: num_key(2); break;	// 2
		case 0b11101101: num_key(3); break;	// 3
		case 0b11101011: num_key(4); break;	// 4
		case 0b11101010: num_key(5); break;	// 5
		case 0b11101001: num_key(6); break;	// 6
		case 0b11100111: num_key(7); break;	// 7
		case 0b11100110: num_key(8); break;	// 8
		case 0b11100101: num_key(9); break;	// 9
		default: return;
	}
	cbi(PORTB,PINB0);
	_delay_ms(50);
	sbi(PORTB,PINB0);
}

void led_blink()
{
	for( uint8_t i=0; i<5; i++ ){
		cbi(PORTD,PIND5);
		_delay_ms(20);
		sbi(PORTD,PIND5);
		_delay_ms(20);
	}
}

ISR(TIMER0_COMPA_vect)
{
	tc += 50;
	if( tc > 60000000 ){
		sleep_mode();
	}
}

ISR(INT0_vect)
{
	volatile uint32_t d0;		// Pulse Off Duration
	volatile uint32_t d1;		// Pulse On Duration
	static uint32_t t0 = 0;
	static uint8_t ph = LEADER;
	static uint8_t bit;			// bit Count
	static uint16_t cc_bits;	// Custom Code
	static uint16_t dt_bits;	// Data Code
	//
	uint32_t d = tc - t0;		// Duration
	uint8_t v = bit_is_clear(PIND,PIND0);
	//
	if( t0 == 0 ){
		t0 = tc;
		return;
	}
	t0 = tc;
	//
	if( v ){
		// Pulse On
		d1 = d;
		// Reader Code: ON:9.0ms, OFF:4.5ms
		if( d0>8500 && d0<9500 && d1>4000 && d1<5000 ){
			ph = DATA;
			cc_bits = 0;
			dt_bits = 0;
			bit = 0;
		}else if( ph == DATA ){
			// Data Code: ON:0.56ms, OFF:0.56ms or 1.56ms
			if( d1>300 && d1<2000 ){
				if( d1 > 1000 ){
					if( bit < 16 ){
						cc_bits |= _BV(bit%16);
					}else{
						dt_bits |= _BV(bit%16);
					}
				}
				bit++;
				if( bit >= 32 ){
					ir_command( dt_bits >> 8 );
					ph = LEADER;
					tc = 0;
				}
			}else{
				ph = LEADER;
				tc = 0;
			}
		}
	}else{
		d0 = d;
	}
}

int main(void)
{
	enable_external_clock();
	// Pin Direction (0:IN,1:OUT)
	DDRB = _BV(PINB0)|_BV(PINB1)|_BV(PINB2)|_BV(PINB3)|_BV(PINB4)|_BV(PINB5)|_BV(PINB6);
	DDRD = _BV(PIND5);
	// Pull Up
	PORTB |= _BV(PINB0);
	PORTD |= _BV(PIND5);
	// INT0
	sbi(EICRA,ISC00);
	cbi(EICRA,ISC01);
	sbi(EIMSK,INT0);
	// TIMER
	TCNT0 = 0;
	OCR0A = 100;
	TCCR0A = _BV(WGM01);	// CTC
	TCCR0B = _BV(CS01);		// 1/8
	TIMSK0 = _BV(OCIE0A);	
	// 
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sei();
	// 
	led_blink();
	// 
	while(1)
	{

	}
}

