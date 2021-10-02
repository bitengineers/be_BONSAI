#pragma once

typedef enum {
  PBHUB_CH0 = 0,
  PBHUB_CH1 = 1,
  PBHUB_CH2 = 2,
  PBHUB_CH3 = 3,
  PBHUB_CH4 = 4,
  PBHUB_CH5 = 5
} pbhub_channel_t;

typedef enum {
  PBHUB_IO0 = 0,
  PBHUB_IO1 = 1,
} pbhub_io_t;

esp_err_t pbhub_init(void);
esp_err_t pbhub_deinit(void);

uint8_t pbhub_digital_read(pbhub_channel_t ch, pbhub_io_t io);
void pbhub_digital_write(pbhub_channel_t ch, pbhub_io_t io, uint8_t value);

uint16_t pbhub_analog_read(pbhub_channel_t ch);
void pbhub_analog_write(pbhub_channel_t ch, pbhub_io_t io, uint16_t value);
