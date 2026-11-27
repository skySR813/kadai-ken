#include "LSM6DSV16X.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define LSM6_ADDR_GND 0x6A
#define LSM6_ADDR_VCC 0x6B
#define LSM6_WHO_AM_I_VALUE 0x70

#define REG_WHO_AM_I 0x0F
#define REG_CTRL1 0x10
#define REG_CTRL2 0x11
#define REG_CTRL3 0x12
#define REG_CTRL6 0x15
#define REG_CTRL8 0x17
#define REG_OUTX_L_G 0x22

#define CTRL3_BDU (1u << 6)
#define CTRL3_IF_INC (1u << 2)

static bool read_registers(
    const lsm6dsv16x_t *sensor, uint8_t start_register, uint8_t *buffer, size_t length)
{
    if (sensor == NULL || sensor->i2c == NULL || buffer == NULL || length == 0) {
        return false;
    }

    if (i2c_write_blocking(sensor->i2c, sensor->address, &start_register, 1, true) != 1) {
        return false;
    }

    return i2c_read_blocking(sensor->i2c, sensor->address, buffer, length, false) ==
           (int)length;
}

static bool write_register(const lsm6dsv16x_t *sensor, uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};

    if (sensor == NULL || sensor->i2c == NULL) {
        return false;
    }

    return i2c_write_blocking(sensor->i2c, sensor->address, data, sizeof(data), false) ==
           (int)sizeof(data);
}

static int16_t decode_int16(const uint8_t *data)
{
    const uint16_t value = (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
    return (int16_t)value;
}

bool lsm6dsv16x_init(
    lsm6dsv16x_t *sensor,
    i2c_inst_t *i2c,
    uint sda_pin,
    uint scl_pin,
    uint32_t baudrate)
{
    uint8_t who_am_i;
    static const uint8_t addresses[] = {LSM6_ADDR_GND, LSM6_ADDR_VCC};

    if (sensor == NULL || i2c == NULL || sda_pin == scl_pin) {
        return false;
    }

    sensor->i2c = i2c;
    sensor->address = 0;

    i2c_init(i2c, baudrate);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);

    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i) {
        sensor->address = addresses[i];
        if (read_registers(sensor, REG_WHO_AM_I, &who_am_i, 1) &&
            who_am_i == LSM6_WHO_AM_I_VALUE) {
            break;
        }
        sensor->address = 0;
    }

    if (sensor->address == 0) {
        return false;
    }

    if (!write_register(sensor, REG_CTRL3, CTRL3_BDU | CTRL3_IF_INC) ||
        !write_register(sensor, REG_CTRL1, 0x09) ||
        !write_register(sensor, REG_CTRL2, 0x09) ||
        !write_register(sensor, REG_CTRL6, 0x04) ||
        !write_register(sensor, REG_CTRL8, 0x82)) {
        sensor->address = 0;
        return false;
    }

    sleep_ms(10);
    return true;
}

bool lsm6dsv16x_read_sensor(
    const lsm6dsv16x_t *sensor, lsm6dsv16x_data_t *data)
{
    uint8_t buffer[12];
    int16_t raw[6];

    if (sensor == NULL || sensor->address == 0 || data == NULL ||
        !read_registers(sensor, REG_OUTX_L_G, buffer, sizeof(buffer))) {
        return false;
    }

    for (size_t i = 0; i < 6; ++i) {
        raw[i] = decode_int16(&buffer[i * 2]);
    }

    data->gx = (float)raw[0] * 0.07f;
    data->gy = (float)raw[1] * 0.07f;
    data->gz = (float)raw[2] * 0.07f;
    data->ax = (float)raw[3] * 0.000244f;
    data->ay = (float)raw[4] * 0.000244f;
    data->az = (float)raw[5] * 0.000244f;
    return true;
}
