#define SYSCLK 11059200

#include <stddef.h>
#include <stdio.h>
#include <c8051F200.h> // SFR declarations
#include <si_toolchain.h>
#include "flash.h"

#include "F200_FlashPrimitives.h"

#pragma NOAREGS

#define TICK_DIVISOR (1152*4)
#define TICKS_IN_SECOND (SYSCLK/12/TICK_DIVISOR)

#define SEGMENT_SIZE 0x200

#define TIME_SLOTS (SEGMENT_SIZE*2/sizeof(SI_UU32_t))
#define LAST_SLOT (TIME_SLOTS-1)
#define PAGE0 0x1800
#define PAGE1 (PAGE0+SEGMENT_SIZE)
#define TIME_SLOT_ADDR(slot) (PAGE0 + slot * sizeof(SI_UU32_t))

volatile SI_UU32_t data time = {0L};
SI_UU32_t code time_slots[TIME_SLOTS] _at_ 0x1800;
volatile int time_slot = 0;

#pragma disable
void save_time(void) using 3 {
	unsigned char b;
	for (b = 0; b < sizeof(SI_UU32_t); b++) {
		FLASH_ByteWrite(TIME_SLOT_ADDR(time_slot) + b, time.u8[b]);
	}
	if (time_slot == 0) {
		FLASH_PageErase(PAGE1);
	} else if (time_slot == TIME_SLOTS/2) {
		FLASH_PageErase(PAGE0);
	}
	time_slot += 1;
	if (time_slot == TIME_SLOTS) {
		time_slot = 0;
	}
}

#pragma disable
void reset_time(void) using 3 {
	if (time_slot < TIME_SLOTS/2) {
		FLASH_PageErase(PAGE0);
	} else {
		FLASH_PageErase(PAGE1);
	}
	time_slot = 0;
	time.u32 = 0L;
	FLASH_ByteWrite(TIME_SLOT_ADDR(0) + 0, 0);
	FLASH_ByteWrite(TIME_SLOT_ADDR(0) + 1, 0);
	FLASH_ByteWrite(TIME_SLOT_ADDR(0) + 2, 0);
	FLASH_ByteWrite(TIME_SLOT_ADDR(0) + 3, 0);
}

#pragma disable
void load_time(void) using 3 {
	int slot;
	time.u32 = 0L;
	time_slot = 0;
	if (time_slots[0].u8[B3] != 0xff) {
		if (time_slots[TIME_SLOTS/2].u8[B3] != 0xff) {
			for (slot = 0; slot < TIME_SLOTS; slot++) {
				if (time_slots[slot].u8[B3] != 0xff ) {
					if (time.u32 < time_slots[slot].u32) {
						time.u32 = time_slots[slot].u32;
						time_slot = slot;
					}
				}
			}
			if (time_slot < TIME_SLOTS/2) {
				FLASH_PageErase(PAGE1);
			} else {
				FLASH_PageErase(PAGE0);
			}
			time_slot += 1;
		} else {
			for (slot = 0; slot < TIME_SLOTS/2; slot++) {
				if (time_slots[slot].u8[B3] != 0xff && time_slots[slot+1].u8[B3] == 0xff ) {
					time.u32 = time_slots[slot].u32;
					time_slot = slot + 1;
					break;
				}
			}
		}
	} else {
		if (time_slots[TIME_SLOTS/2].u8[B3] != 0xff) {
			for (slot = TIME_SLOTS/2; slot < TIME_SLOTS; slot++) {
				if (time_slots[slot].u8[B3] != 0xff && (slot == LAST_SLOT || time_slots[slot+1].u8[B3] == 0xff) ) {
					time.u32 = time_slots[slot].u32;
					time_slot = slot + 1;
					break;
				}
			}
		} // else do nothing, both segments uninitialized
	}
	if (time_slot == TIME_SLOTS) {
		time_slot = 0;
	}
}



//#define OTHER_BLOCK(block) ((block)==0?1:0)

//#define BLOCK_SIG 0xB4

// 512 byte block
// 4000 minutes
// block: 00 B4 00 00 xx xx [500 bytes one bit per minute ...0000111111...] FF FF FF FF FF FF
// xx xx number of blocks

/*
#define MINUTE_BIT_BYTES 500
#define MINUTES_IN_BLOCK ((MINUTE_BIT_BYTES)*8)
#define FILLER_BYTES 6

#define BLOCK_ADDR(block_id) (0x1800 + (block_id)*SEGMENT_SIZE)
#define BLOCK_VALID(block_id) (blocks[block_id].header.lead_in == 0 && blocks[block_id].header.sig == BLOCK_SIG)
*/

/*
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
*/

