#include "tm1637.h"

#include "pico/stdlib.h"

#define TM1637_DIGITS 4
#define TM1637_BRIGHTNESS 2
#define DIGIT_OR_ALPHABET 0  // 0 for digits, 1 for hexadecimal alphabet

static const uint8_t digit_segments[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66,
    0x6d, 0x7d, 0x07, 0x7f, 0x6f
};

static const uint8_t hexadecimal_segments[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66,
    0x6d, 0x7d, 0x07, 0x7f, 0x6f,
    0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71
};

static void tm1637_delay(void)
{
    sleep_us(5);
}

static void tm1637_start(const tm1637_config_t *display)
{
    gpio_put(display->clk_pin, 1);
    gpio_put(display->dio_pin, 1);
    tm1637_delay();
    gpio_put(display->dio_pin, 0);
    tm1637_delay();
    gpio_put(display->clk_pin, 0);
}

static void tm1637_stop(const tm1637_config_t *display)
{
    gpio_put(display->clk_pin, 0);
    gpio_put(display->dio_pin, 0);
    tm1637_delay();
    gpio_put(display->clk_pin, 1);
    tm1637_delay();
    gpio_put(display->dio_pin, 1);
    tm1637_delay();
}

static void tm1637_write_byte(const tm1637_config_t *display, uint8_t value)
{
    for (int bit = 0; bit < 8; ++bit) {
        gpio_put(display->clk_pin, 0);
        gpio_put(display->dio_pin, value & 1);
        tm1637_delay();
        gpio_put(display->clk_pin, 1);
        tm1637_delay();
        value >>= 1;
    }

    gpio_put(display->clk_pin, 0);
    gpio_set_dir(display->dio_pin, GPIO_IN);
    tm1637_delay();
    gpio_put(display->clk_pin, 1);
    tm1637_delay();
    gpio_put(display->clk_pin, 0);
    gpio_put(display->dio_pin, 0);
    gpio_set_dir(display->dio_pin, GPIO_OUT);
}

void tm1637_init(tm1637_config_t *display, uint8_t clk_pin, uint8_t dio_pin)
{
    display->clk_pin = clk_pin;
    display->dio_pin = dio_pin;

    gpio_init(display->clk_pin);
    gpio_set_dir(display->clk_pin, GPIO_OUT);
    gpio_put(display->clk_pin, 1);

    gpio_init(display->dio_pin);
    gpio_set_dir(display->dio_pin, GPIO_OUT);
    gpio_put(display->dio_pin, 1);
    gpio_pull_up(display->dio_pin);
}

void tm1637_display_number(const tm1637_config_t *display, int16_t number)
{
    uint8_t segments[TM1637_DIGITS];

    if (DIGIT_OR_ALPHABET == 0) {
        const bool negative = number < 0;
        uint16_t magnitude = (uint16_t)(negative ? -(int32_t)number : number);

        if (negative) {
            for (int digit = TM1637_DIGITS - 1; digit > 0; --digit) {
                segments[digit] = digit_segments[magnitude % 10];
                magnitude /= 10;
            }
            segments[0] = 0x40;
        } else {
            magnitude %= 10000;
            for (int digit = TM1637_DIGITS - 1; digit >= 0; --digit) {
                segments[digit] = digit_segments[magnitude % 10];
                magnitude /= 10;
            }
        }
    } else {
        uint16_t magnitude = (uint16_t)number;
        for (int digit = TM1637_DIGITS - 1; digit >= 0; --digit) {
            segments[digit] = hexadecimal_segments[magnitude & 0x0f];
            magnitude >>= 4;
        }
    }

    tm1637_start(display);
    tm1637_write_byte(display, 0x40);
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, 0xc0);
    for (int digit = 0; digit < TM1637_DIGITS; ++digit) {
        tm1637_write_byte(display, segments[digit]);
    }
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, 0x88 | TM1637_BRIGHTNESS);
    tm1637_stop(display);
}
