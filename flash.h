#ifndef LCD_FLASH_H
#define LCD_FLASH_H

#define LOWBYTE(v)   ((unsigned char) (v))
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))

//unsigned int load_block_minutes(unsigned char block_id);
//void set_current_block();
void register_minute(void);
//extern unsigned char current_block;
extern unsigned int minute;

#endif // LCD_FLASH_H