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

const unsigned char digit_lookup[16] = {
0x3f, //0b00111111, //0
0x03, //0b00000011, //1
0x76, //0b01110110, //2
0x67, //0b01100111, //3
0x4b, //0b01001011, //4
0x6d, //0b01101101, //5
0x7d, //0b01111101, //6
0x07, //0b00000111, //7
0x7f, //0b01111111, //8
0x6f, //0b01101111, //9
0x5f, //0b01011111, //A
0x79, //0b01111001, //B
0x3c, //0b00111100, //C
0x73, //0b01110011, //D
0x7c, //0b01111100, //E
0x5c  //0b01011100, //F
};

#define MAX_RESET_STEPS 8

const unsigned int reset_display_lookup[MAX_RESET_STEPS] = {
0x0004,
0x0006,
0x0007,
0x0027,
0x2027,
0x3027,
0x3827,
0x3C27
};

void display_second(void) {
	one = 0;
	display_blink = (second > 49);
	display[0] = digit_lookup[second % 10];
	if (second > 9) {
		display[1] = digit_lookup[second / 10];
	} else {
		display[1] = 0;
	}
}


void display_minute(void) {
	display_blink = (minute > 199);
	if (display_blink) {
		one = 1;
		display[0] = digit_lookup[9];
		display[1] = digit_lookup[9];
	} else {
		unsigned int partial = minute;
		if (minute > 99) {
			partial -= 100;
			one = 1;
		} else {
			one = 0;
		}
		display[0] = digit_lookup[partial % 10];
		display[1] = digit_lookup[partial / 10];
		if (!one && partial < 10) { // blank out tens if it is zero and there is no hundred
			display[1] = 0;
		}
	}   
}


sbit P1_4 = P1 ^ 4;
sbit P1_5 = P1 ^ 5;
sbit P1_6 = P1 ^ 6;
sbit P1_7 = P1 ^ 7;

// bit 6
#define POWER_OK (CPT0CN & 0x40)

#define COUNTING (P1_4 == 0)
#define RESETTING (P1_5 == 0)
#define MAX_TIME_SAVE_DEBOUNCE 10
unsigned char time_save_debounce = MAX_TIME_SAVE_DEBOUNCE;
unsigned char reset_hold_step = 0;

void Timer2_ISR (void) interrupt INTERRUPT_TIMER2 using 1 {
	// every tick	

//	P1_7 = 0;
	if (display_on) {
		unsigned char inv = ((com) ? 0xff : 0x00);
		P2 = ((display[0] ^ inv) & 0x7F) | (com ? 0x80 : 0x00);
		P3 = (((display[1] & 0x7F) | (one ? 0x80 : 0x00)) ^ inv);
	} else {
		P2 = 0;
		P3 = 0;
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
				reset_hold_step += 1;
				if (reset_hold_step == MAX_RESET_STEPS && POWER_OK) {
					reset_time();
					minute = 0;
					second = 0;
				}
			}
		}
	} else {
		reset_hold_step = 0;
		if (tick == 0) {
			if (second == 0) {
				display_minute();
			} else {
				display_second();
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
   second = time.u32 % 60L;
   minute = time.u32 / 60L;

   while (1);
}

