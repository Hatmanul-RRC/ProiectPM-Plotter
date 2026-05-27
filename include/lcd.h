#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(uint16_t color);
void lcd_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void lcd_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color, uint8_t size);

#endif
