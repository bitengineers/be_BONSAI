#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_pahub.h"

#define PAHUB_TAG "PAHUB"

#define PAHUB_SDA (GPIO_NUM_32)
#define PAHUB_SCL (GPIO_NUM_33)
#define PAHUB_I2C_CLK (400 * 1000)
#define PAHUB_I2C I2C_NUM_1
#define PAHUB_I2C_ADDR (0x70)

static esp_err_t pahub_write_reg(uint8_t value);
static esp_err_t pahub_read_reg(uint8_t *value);

esp_err_t pahub_init(void)
{
  esp_err_t err;
  i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PAHUB_SDA,
    .scl_io_num = PAHUB_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = PAHUB_I2C_CLK
  };
  err = i2c_param_config(PAHUB_I2C, &i2c_config);
  if (err != ESP_OK) {
    return err;
  }
  err = i2c_driver_install(PAHUB_I2C, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK) {
    return err;
  }
  i2c_set_timeout(PAHUB_I2C, pdMS_TO_TICKS(3000));
  return ESP_OK;
}

esp_err_t pahub_deinit(void)
{
  return i2c_driver_delete(PAHUB_I2C);
}

esp_err_t pahub_ch(uint8_t channel)
{
  return pahub_write_reg(channel);
}

static esp_err_t pahub_write_reg(uint8_t value)
{
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (PAHUB_I2C_ADDR<<1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, value, true);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(PAHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);
  ESP_LOGI(PAHUB_TAG, "pahub_write_reg returns %d\n", err);
  return err;
}

static esp_err_t pahub_read_reg(uint8_t *value)
{
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (PAHUB_I2C_ADDR<<1) | I2C_MASTER_READ, true);
  i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(PAHUB_I2C, cmd, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(cmd);
  ESP_LOGI(PAHUB_TAG, "pahub_read_reg returns %d\n", err);
  return err;
}
