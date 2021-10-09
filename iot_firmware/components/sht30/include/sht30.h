#pragma once

#include "esp_err.h"

esp_err_t sht30_init(void);
esp_err_t sht30_deinit(void);

esp_err_t sht30_start_measurement(void);
esp_err_t sht30_wait_measurement(void);
esp_err_t sht30_read_measured_values(uint16_t *temperature, uint16_t *humidity);
float sht30_calc_celsius(uint16_t temp);
float sht30_calc_relative_humidity(uint16_t hum);
esp_err_t sht30_heater(bool b);
