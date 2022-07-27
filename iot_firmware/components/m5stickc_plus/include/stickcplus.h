#pragma once

#include <stdbool.h>

#include "esp_err.h"

#define M5STICKC_PLUS_SDA      21
#define M5STICKC_PLUS_SCL      22
#define M5STICKC_PLUS_I2C      (I2C_NUM_0)

extern bool stickcplus_is_initialized;

esp_err_t stickcplus_init(void);
esp_err_t stickcplus_deinit(void);

esp_err_t stickcplus_i2c_init(void);
esp_err_t stickcplus_i2c_deinit(void);

bool stickcplus_i2c_is_initialized(void);
