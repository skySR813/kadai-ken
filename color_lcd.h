#ifndef COLOR_LCD_H
#define COLOR_LCD_H

#include <stdint.h>

void color_lcd_init(void);
void color_lcd_display_number(uint16_t number, uint16_t color);

#endif
