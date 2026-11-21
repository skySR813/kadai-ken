#include "color_lcd.h"

#include "hardware/spi.h"
#include "pico/stdlib.h"

#define SPI_PORT spi0
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_DC   20
#define PIN_RST  21

#define LCD_WIDTH  240
#define LCD_HEIGHT 240
#define DIGIT_SCALE 5
#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 7
#define DIGIT_GAP 1
#define DIGITS_TO_DISPLAY 4

static const uint8_t digit_glyphs[10][DIGIT_HEIGHT] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
    {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
    {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
    {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
    {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
    {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}
};

static void lcd_write_command(uint8_t command)
{
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_PORT, &command, 1);
    gpio_put(PIN_CS, 1);
}

static void lcd_write_data(const uint8_t *data, size_t length)
{
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    spi_write_blocking(SPI_PORT, data, length);
    gpio_put(PIN_CS, 1);
}

static void lcd_write_command_data(uint8_t command, const uint8_t *data, size_t length)
{
    lcd_write_command(command);
    lcd_write_data(data, length);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    lcd_write_command_data(0x2A, data, sizeof(data));

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    lcd_write_command_data(0x2B, data, sizeof(data));

    lcd_write_command(0x2C);
}

static void lcd_init_controller(void)
{
    gpio_put(PIN_RST, 1);
    sleep_ms(10);
    gpio_put(PIN_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);

    lcd_write_command(0x01);
    sleep_ms(150);
    lcd_write_command(0x11);
    sleep_ms(120);

    const uint8_t pixel_format = 0x55;
    lcd_write_command_data(0x3A, &pixel_format, 1);

    const uint8_t memory_access = 0x00;
    lcd_write_command_data(0x36, &memory_access, 1);

    lcd_write_command(0x21);
    lcd_write_command(0x13);
    sleep_ms(10);
    lcd_write_command(0x29);
    sleep_ms(100);
}

static bool lcd_pixel_is_digit(uint16_t x, uint16_t y, uint16_t number)
{
    const uint16_t digit_stride = (DIGIT_WIDTH + DIGIT_GAP) * DIGIT_SCALE;
    const uint16_t text_width =
        (DIGITS_TO_DISPLAY * DIGIT_WIDTH + (DIGITS_TO_DISPLAY - 1) * DIGIT_GAP) *
        DIGIT_SCALE;
    const uint16_t text_height = DIGIT_HEIGHT * DIGIT_SCALE;
    const uint16_t origin_x = (LCD_WIDTH - text_width) / 2;
    const uint16_t origin_y = (LCD_HEIGHT - text_height) / 2;

    if (x < origin_x || x >= origin_x + text_width ||
        y < origin_y || y >= origin_y + text_height) {
        return false;
    }

    const uint16_t local_x = x - origin_x;
    const uint16_t column = (local_x / DIGIT_SCALE) % (DIGIT_WIDTH + DIGIT_GAP);
    const uint16_t row = (y - origin_y) / DIGIT_SCALE;
    if (column >= DIGIT_WIDTH) {
        return false;
    }

    uint16_t divisor = 1;
    for (uint16_t i = 0; i < DIGITS_TO_DISPLAY - 1 - local_x / digit_stride; ++i) {
        divisor *= 10;
    }
    const uint8_t digit = (uint8_t)((number / divisor) % 10);
    return (digit_glyphs[digit][row] & (1u << (DIGIT_WIDTH - 1 - column))) != 0;
}

static bool lcd_pixel_in_circle(uint16_t x, uint16_t y)
{
    const int32_t dx = (int32_t)x - 58;
    const int32_t dy = (int32_t)y - 52;
    return dx * dx + dy * dy <= 23 * 23;
}

static bool lcd_pixel_on_rectangle(uint16_t x, uint16_t y)
{
    const uint16_t left = 145;
    const uint16_t top = 29;
    const uint16_t right = 210;
    const uint16_t bottom = 75;
    const uint16_t border = 5;

    return x >= left && x <= right && y >= top && y <= bottom &&
           (x - left < border || right - x < border ||
            y - top < border || bottom - y < border);
}

static int32_t triangle_edge(
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x, int32_t y)
{
    return (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
}

static bool lcd_pixel_in_triangle(uint16_t x, uint16_t y)
{
    const int32_t px = x;
    const int32_t py = y;
    const int32_t edge1 = triangle_edge(120, 163, 87, 220, px, py);
    const int32_t edge2 = triangle_edge(87, 220, 153, 220, px, py);
    const int32_t edge3 = triangle_edge(153, 220, 120, 163, px, py);

    return (edge1 >= 0 && edge2 >= 0 && edge3 >= 0) ||
           (edge1 <= 0 && edge2 <= 0 && edge3 <= 0);
}

static bool lcd_pixel_on_line(uint16_t x, uint16_t y){
    const int32_t x1 = 50;
    const int32_t y1 = 150;
    const int32_t x2 = 190;
    const int32_t y2 = 150;
    const int32_t dx = x2 - x1;
    const int32_t dy = y2 - y1;
    const int32_t cross = (x - x1) * dy - (y - y1) * dx;
    return cross == 0 && x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

static bool lcd_pixel_on_arrow(uint16_t x, uint16_t y){
    const int32_t x1 = 50;
    const int32_t x2 = 190;
    const int32_t x3 = 100;
    const int32_t y1 = 140;
    const int32_t y2 = 160;
    const int32_t y3 = 150;
    const int32_t dx = x2 - x1;
    const int32_t dy = 
}

static uint16_t lcd_pixel_color(uint16_t x, uint16_t y, uint16_t number, uint16_t number_color)
{
    if (lcd_pixel_in_circle(x, y)) {
        return 0x07FF;
        //return number_color + 1;
    }
    if (lcd_pixel_on_rectangle(x, y)) {
        //return number_color + 2;
        return 0xF800;
    }
    if (lcd_pixel_in_triangle(x, y)) {
        //return number_color + 3;
        return 0xFFE0;
    }
    if (lcd_pixel_on_line(x, y)) {
        return 0xF81F;
    }
    if (lcd_pixel_is_digit(x, y, number)) {
        return number_color;
    }
    return 0x0000;
}

void color_lcd_init(void)
{
    spi_init(SPI_PORT, 10 * 1000 * 1000);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    lcd_init_controller();
}

void color_lcd_display_number(uint16_t number, uint16_t color)
{
    uint8_t line[LCD_WIDTH * 2];
    number %= 10000;

    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    for (uint16_t y = 0; y < LCD_HEIGHT; ++y) {
        for (uint16_t x = 0; x < LCD_WIDTH; ++x) {
            const uint16_t pixel = lcd_pixel_color(x, y, number, color);
            line[x * 2] = (uint8_t)(pixel >> 8);
            line[x * 2 + 1] = (uint8_t)pixel;
        }
        lcd_write_data(line, sizeof(line));
    }
}
