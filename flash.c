#include <stddef.h>
#include <stdio.h>
#include <c8051f200.h> // SFR declarations
#include "flash.h"


#define SYSCLK 11059200
#define TICK_DIVISOR (1152*4)
#define TICKS_IN_SECOND (SYSCLK/12/TICK_DIVISOR)
#define OTHER_BLOCK(block) ((block)==0?1:0)

#define BLOCK_SIG 0xB4

// 512 byte block
// 4000 minutes
// block: 00 B4 00 00 xx xx [500 bytes one bit per minute ...0000111111...] FF FF FF FF FF FF
// xx xx number of blocks

#define BLOCK_SIZE 0x200
#define MINUTE_BIT_BYTES 500
#define MINUTES_IN_BLOCK ((MINUTE_BIT_BYTES)*8)
#define FILLER_BYTES 6

#define BLOCK_ADDR(block_id) (0x1800 + (block_id)*BLOCK_SIZE)
#define BLOCK_VALID(block_id) (blocks[block_id].header.lead_in == 0 && blocks[block_id].header.sig == BLOCK_SIG)

#include "F200_FlashPrimitives.h"

typedef struct {
    struct {
      unsigned char lead_in;
      unsigned char sig;
      unsigned char unused[2];
      unsigned short block_base;
    } header;
    unsigned char minute_bits[MINUTE_BIT_BYTES];
    unsigned char filler[FILLER_BYTES];
} block_type;

block_type code blocks[2] _at_ BLOCK_ADDR(0);

volatile unsigned char current_block = 2;
volatile unsigned int minute = 0;

#pragma NOAREGS
void load_minute(void) {
  block_type *block_ptr = &blocks[current_block];
  unsigned int minutes = block_ptr->header.block_base;
  unsigned int bi;
  unsigned char b;
  for (bi = 0; bi < MINUTE_BIT_BYTES; bi++) {
    b = block_ptr->minute_bits[bi];
    if (b == 0) {
      minutes += 8;
    } else if (b < 0xff) {
      while ((b & 0x80) == 0) {
        minutes ++;
        b <<= 1;
      }
      break;
    }
  }
  minute = minutes;
}

void erase_block(unsigned char block_id) {
	FLASH_PageErase(BLOCK_ADDR(block_id));
}

void write_block_byte(unsigned char block_id, unsigned int index, unsigned char byte) {
	FLASH_ByteWrite(BLOCK_ADDR(block_id)+index, byte);
}

void write_block_word(unsigned char block_id, unsigned int index, unsigned short word) {
	FLASH_ByteWrite(BLOCK_ADDR(block_id)+index, HIGHBYTE(word));
	FLASH_ByteWrite(BLOCK_ADDR(block_id)+index+1, LOWBYTE(word));
}

void initialize_block(unsigned char block_id, unsigned int block_base) {
  erase_block(block_id);
  write_block_byte(block_id, 0, 0x00);
  write_block_byte(block_id, offsetof(block_type, header.unused[0]), 0);
  write_block_byte(block_id, offsetof(block_type, header.unused[1]), 0);
  write_block_word(block_id, offsetof(block_type, header.block_base), block_base);
  write_block_byte(block_id, offsetof(block_type, header.sig), BLOCK_SIG);
}

void set_current_block() {
  // find most recent block
  if (BLOCK_VALID(0)) {
    if (BLOCK_VALID(1)) {
      // both blocks appear valid, find the most recent one
      if (blocks[0].header.block_base >= blocks[1].header.block_base) {
        current_block = 0;
      } else {
        current_block = 1;
      }
    } else {
      current_block = 0;
    }
  } else {
    if (BLOCK_VALID(1)) {
      current_block = 1;
    } else {
      initialize_block(0, 0);
	  current_block = 0;
    }
  }
}


void register_minute(void) {
	if (current_block > 1) {
		set_current_block();
		load_minute();
//		P1_7 = 0;
	} else {
	  block_type *block_ptr = &blocks[current_block];
	  unsigned int bi;
	  unsigned char b;
	  for (bi = 0; bi < MINUTE_BIT_BYTES; bi++) {
	    b = block_ptr->minute_bits[bi];
	    if (b > 0) {
//		  P1_7 = 1;
	      write_block_byte(current_block, offsetof(block_type, minute_bits) + bi, b >> 1);
//		  P1_7 = 0;
	      break;
	    }
	  }
	  if (bi == MINUTE_BIT_BYTES) {
	    // current block full
		unsigned int base = block_ptr->header.block_base + MINUTES_IN_BLOCK;
	    current_block = OTHER_BLOCK(current_block);
	    block_ptr = &blocks[current_block];
	    initialize_block(current_block, base);
	    write_block_byte(current_block, offsetof(block_type, minute_bits), 0x7f);
	  }
	}
}
