#ifndef TM1637_H
#define TM1637_H

#include <stdint.h>

typedef struct {
    uint8_t clk_pin;
    uint8_t dio_pin;
} tm1637_config_t;

void tm1637_init(tm1637_config_t *display, uint8_t clk_pin, uint8_t dio_pin);
void tm1637_display_number(const tm1637_config_t *display, int16_t number);

#endif
