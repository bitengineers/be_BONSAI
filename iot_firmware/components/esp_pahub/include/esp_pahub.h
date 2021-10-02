#pragma once

#include "esp_err.h"

#define PAHUB_ENABLE_CH0     0x01
#define PAHUB_ENABLE_CH1     0x02
#define PAHUB_ENABLE_CH2     0x04
#define PAHUB_ENABLE_CH3     0x08
#define PAHUB_ENABLE_CH4     0x10
#define PAHUB_ENABLE_CH5     0x20
#define PAHUB_ENABLE_CH6     0x40
#define PAHUB_ENABLE_CH7     0x80
#define PAHUB_ENABLE_CH_ALL  0xFF
#define PAHUB_DISABLE_CH_ALL 0x00

esp_err_t pahub_init(void);
esp_err_t pahub_deinit(void);
esp_err_t pahub_ch(uint8_t channel);
