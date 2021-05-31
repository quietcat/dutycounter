#include <intrins.h>
#include <c8051f200.h> // SFR declarations
#include <si_toolchain.h>
#include "flash.h"
#include "F200_FlashPrimitives.h"

#define INTERRUPT_TIMER2               5  // Timer2 Overflow
#define INTERRUPT_CP0_FALLING			10 // CP0 falling edge

#define TICKS_PER_SECOND 200

volatile unsigned char tick = 0;
volatile unsigned char second = 0;
volatile unsigned char minute = 0;

volatile unsigned char display[2] = {0x40,0x40};

volatile bit com = 0;
volatile bit display_on = 1;
volatile bit one = 0;
volatile bit display_blink = 0;
volatile bit polarity = 0;

#include "segments.h"

const unsigned char digit0_lookup[16] = {
D0_0,
D0_1,
D0_2,
D0_3,
D0_4,
D0_5,
D0_6,
D0_7,
D0_8,
D0_9,
D0_A,
D0_B,
D0_C,
D0_D,
D0_E,
D0_F,
};

const unsigned char digit1_lookup[16] = {
D1_0,
D1_1,
D1_2,
D1_3,
D1_4,
D1_5,
D1_6,
D1_7,
D1_8,
D1_9,
D1_A,
D1_B,
D1_C,
D1_D,
D1_E,
D1_F,
};

#define LCD_COM (1 << 4)
#define LCD_COM_MASK (~LCD_COM)
#define LCD_1 (1 << 0)
#define LCD_1_MASK (~LCD_1)



#define MAX_RESET_STEPS 8

const unsigned int reset_display_lookup[MAX_RESET_STEPS] = {
	D0_SEGA,
	D0_SEGA | D0_SEGB,
	D0_SEGA | D0_SEGB | D0_SEGC,
	D0_SEGA | D0_SEGB | D0_SEGC | D0_SEGD,
	D0_SEGA | D0_SEGB | D0_SEGC | D0_SEGD | ((D1_SEGD) << 8),
	D0_SEGA | D0_SEGB | D0_SEGC | D0_SEGD | ((D1_SEGD | D1_SEGE) << 8),
	D0_SEGA | D0_SEGB | D0_SEGC | D0_SEGD | ((D1_SEGD | D1_SEGE | D1_SEGF) << 8),
	D0_SEGA | D0_SEGB | D0_SEGC | D0_SEGD | ((D1_SEGD | D1_SEGE | D1_SEGF | D1_SEGA) << 8)
};

sbit P1_6 = P1 ^ 6;
sbit P1_7 = P1 ^ 7;

// bit 6
#define POWER_OK (CPT0CN & 0x40)

#define CNT_INPUT (CPT1CN & 0x40)
#define COUNTING (!CNT_INPUT != !polarity)
#define RESETTING (P1_6 == 0)
#define MAX_TIME_SAVE_DEBOUNCE 10
unsigned char time_save_debounce = MAX_TIME_SAVE_DEBOUNCE;
unsigned char reset_hold_step = 0;
unsigned char duty_counter = 0;

// how many seconds on one duty unit (3 hours)
#define DUTY_UNIT_DIVIDER (60L*60L*3L)
#define CALCULATE_DUTY (duty_counter = (time.u32 > 199L*DUTY_UNIT_DIVIDER) ? 199 : time.u32/DUTY_UNIT_DIVIDER)

void display_second(void) {
	one = 0;
	display_blink = (second > 49);
	display[0] = digit0_lookup[second % 10];
	if (second > 9) {
		display[1] = digit1_lookup[second / 10];
	} else {
		display[1] = 0;
	}
}


void display_minute(void) {
	display_blink = (minute > 199);
	if (display_blink) {
		one = 1;
		display[0] = digit0_lookup[9];
		display[1] = digit1_lookup[9];
	} else {
		unsigned int partial = minute;
		one = (minute > 99);
		if (one) {
			partial = minute % 100;
		}
		display[0] = digit0_lookup[partial % 10];
		if (!one && partial < 10) { // blank out tens if it is zero and there is no hundred
			display[1] = 0;
		} else {
			display[1] = digit1_lookup[partial / 10];
		}
	}   
}

void display_duty(void) {
	unsigned char partial = duty_counter;
	one = (duty_counter > 99);
	if (one) {
		partial = duty_counter % 100;
	}
	display_blink = one;
	display[0] = digit0_lookup[partial % 10];
	if (!one && partial < 10) { // blank out tens if it is zero and there is no hundred
		display[1] = 0;
	} else {
		display[1] = digit1_lookup[partial / 10];
	}
}

void Timer2_ISR (void) interrupt INTERRUPT_TIMER2 using 1 {
	// every tick	

//	P1_7 = 0;
	if (display_on) {
		unsigned char inv = ((com) ? 0xff : 0x00);
//		P0 = ((display[0] ^ inv) & 0x7F) | (com ? 0x40 : 0x00);
//		P2 = (((display[1] & 0x7F) | (one ? 0x80 : 0x00)) ^ inv);
		P0 = ( ( (display[0] & LCD_1_MASK) | (one ? LCD_1 : 0x00) ) ^ inv );
		// com bit will also be xor'd, so it should be reversed
		P2 = ( (display[1] ^ inv) | (com ? LCD_COM : 0x00 ) );
	} else {
		P0 = 0;
		P2 = 0;
	}

	com = !com;

	if (display_blink) {
		switch (tick) {
		case 0: display_on = 1; break;
		case 100: display_on = 0; break;
		}
	} else {
		display_on = 1;
	}

	if (time_save_debounce < MAX_TIME_SAVE_DEBOUNCE && !COUNTING) {
		time_save_debounce += 1;
		if (time_save_debounce == MAX_TIME_SAVE_DEBOUNCE && POWER_OK) {
			save_time();
			CALCULATE_DUTY;
			minute = 0;
			second = 0;
		}
	}

	if (RESETTING && !COUNTING) {
		if (reset_hold_step == MAX_RESET_STEPS) {
			display_blink = 1;
			// reset
		} else {
			display_blink = 0;
			if (tick == 0 || tick == 100) {
				SI_UU16_t d;
				d.u16 = reset_display_lookup[reset_hold_step];
				display[0] = d.u8[LSB];
				display[1] = d.u8[MSB];
				one = 0;
				reset_hold_step += 1;
				if (reset_hold_step == MAX_RESET_STEPS && POWER_OK) {
					reset_time();
					duty_counter = 0;
				}
			}
		}
	} else {
		reset_hold_step = 0;
		if (tick == 0) {
			if (COUNTING) {
				if (second == 0) {
					display_minute();
				} else {
					display_second();
				}
			} else {
				display_duty();
			}
		}
	}

	if (++tick == TICKS_PER_SECOND) {
		// once every second
		tick = 0;
		if ( COUNTING ) {
			time.u32 += 1;
			time_save_debounce = 0;
	
			if (++second == 60) {
				// once every minute
				second = 0;
				minute++;
			}
		}
	}


//	P1_7 = 1;
	TF2 = 0;
}

// power fail
void CP0_Falling_ISR (void) interrupt INTERRUPT_CP0_FALLING using 2 {
	
	if ( COUNTING ) {
		save_time();
	}
	CPT0CN    &= ~0x30; // CP0FIF = 0 and CP0RIF = 0;
}

void Init_Device(void);

void main (void) {

   WDTCN = 0xde;                       // disable watchdog timer
   WDTCN = 0xad;

   Init_Device();
   load_time();
   CALCULATE_DUTY;
   polarity = CNT_INPUT;

   while (1);
}

