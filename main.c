#include <intrins.h>
#include <c8051f200.h> // SFR declarations
#include "flash.h"
#include "F200_FlashPrimitives.h"

#define INTERRUPT_TIMER2               5  // Timer2 Overflow
#define INTERRUPT_CP0_FALLING			10 // CP0 falling edge

#define TICKS_PER_SECOND 200

volatile unsigned char tick = 0;
volatile unsigned char second = 0;

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


sbit P1_6 = P1 ^ 6;
sbit P1_7 = P1 ^ 7;


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

	if (tick == 0) {
		if (second == 0) {
//			register_minute();
			display_minute();
		} else {
			display_second();
		}
	}

	if (++tick == TICKS_PER_SECOND) {
		// once every second
		tick = 0;

		if (++second == 60) {
			// once every minute
			second = 0;
			minute++;
		}
	}


//	P1_7 = 1;
	TF2 = 0;
}

void CP0_Falling_ISR (void) interrupt INTERRUPT_CP0_FALLING using 2 {
	
	if (!(minute == 0 && second == 0)) {
		P1_6 = 0;
		FLASH_PageErase(0x1800);
		FLASH_ByteWrite (0x1800, 0xAA);
		FLASH_ByteWrite (0x1801, 0xBB);
		FLASH_ByteWrite (0x1802, 0xCC);
		FLASH_ByteWrite (0x1803, 0xDD);
		P1_6 = 1;
	}
	CPT0CN    &= ~0x30; // CP0FIF = 0 and CP0RIF = 0;
}

void Init_Device(void);

void main (void) {

   WDTCN = 0xde;                       // disable watchdog timer
   WDTCN = 0xad;

   Init_Device();

   while (1);
}

