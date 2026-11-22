#ifndef LSM6DSV16X_H
#define LSM6DSV16X_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
} lsm6dsv16x_t;

typedef struct {
    float gx;
    float gy;
    float gz;
    float ax;
    float ay;
    float az;
} lsm6dsv16x_data_t;

bool lsm6dsv16x_init(
    lsm6dsv16x_t *sensor,
    i2c_inst_t *i2c,
    uint sda_pin,
    uint scl_pin,
    uint32_t baudrate);

bool lsm6dsv16x_read_sensor(
    const lsm6dsv16x_t *sensor, lsm6dsv16x_data_t *data);

#endif
