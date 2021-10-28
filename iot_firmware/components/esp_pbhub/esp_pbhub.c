#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_pbhub.h"

#define PBHUB_TAG "PBHUB"
#define PBHUB_SDA (GPIO_NUM_32)
#define PBHUB_SCL (GPIO_NUM_33)
#define PBHUB_I2C_CLK (400 * 1000)
#define PBHUB_I2C I2C_NUM_1
#define PBHUB_I2C_ADDR (0x61)

const uint8_t PB_READ_DIGITAL[6][2] = {
  { 0x44, 0x45 },
  { 0x54, 0x55 },
  { 0x64, 0x65 },
  { 0x74, 0x75 },
  { 0x84, 0x85 },
  { 0xa4, 0xa5 }
};
const uint8_t PB_WRITE_DIGITAL[6][2] = { 
  { 0x40, 0x41 },
  { 0x50, 0x51 },
  { 0x60, 0x61 },
  { 0x70, 0x71 },
  { 0x80, 0x81 },
  { 0xa0, 0xa1 }
};
const uint8_t PB_READ_ANALOG[6] = { 
  0x46,
  0x56,
  0x66,
  0x76,
  0x86,
  0xa6
};
const uint8_t PB_WRITE_ANALOG[6][2] = { 
  { 0x42, 0x43 },
  { 0x52, 0x53 },
  { 0x62, 0x63 },
  { 0x72, 0x73 },
  { 0x82, 0x83 },
  { 0xa2, 0xa3 }
};

esp_err_t pbhub_init(void)
{
  esp_err_t err;
  i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PBHUB_SDA,
    .scl_io_num = PBHUB_SCL,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = PBHUB_I2C_CLK
  };
  err = i2c_param_config(PBHUB_I2C, &i2c_config);
  if (err != ESP_OK) {
    return err;
  }
  err = i2c_driver_install(PBHUB_I2C, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK) {
    return err;
  }
  i2c_set_timeout(PBHUB_I2C, pdMS_TO_TICKS(3000));
  return ESP_OK;
}

esp_err_t pbhub_deinit(void)
{
  return i2c_driver_delete(PBHUB_I2C);
}

uint8_t pbhub_digital_read(pbhub_channel_t ch, pbhub_io_t io)
{
  uint8_t value = PB_READ_DIGITAL[ch][io];
  uint8_t v = 0x00;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, PBHUB_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, value, true);
  i2c_master_stop(cmd);
  i2c_master_start(cmd);
  i2c_master_read_byte(cmd, &v, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(PBHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);

  return v;
}

void pbhub_digital_write(pbhub_channel_t ch, pbhub_io_t io, uint8_t value)
{
  uint8_t v = PB_WRITE_DIGITAL[ch][io];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, PBHUB_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, v, true);
  i2c_master_write_byte(cmd, value, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(PBHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);
}

uint16_t pbhub_analog_read(pbhub_channel_t ch)
{
  esp_err_t err;
  uint8_t v = PB_READ_ANALOG[ch];
  uint8_t r[2] = { 0x00, 0x00 };
  uint16_t rtn;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, PBHUB_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, v, true);
  // i2c_master_stop(cmd);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, PBHUB_I2C_ADDR << 1 | I2C_MASTER_READ, true);
  i2c_master_read_byte(cmd, r, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, r+1, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(PBHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);
  ESP_LOGI(PBHUB_TAG, "pbhub_analog_read: I2C returns %d", err);
  rtn = r[0] + (r[1] << 8);
  return rtn;
}

void pbhub_analog_write(pbhub_channel_t ch, pbhub_io_t io, uint16_t value)
{
  uint8_t v = PB_WRITE_ANALOG[ch][io];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, PBHUB_I2C_ADDR << 1 | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, v, true);
  i2c_master_write_byte(cmd, value, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(PBHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);
}
