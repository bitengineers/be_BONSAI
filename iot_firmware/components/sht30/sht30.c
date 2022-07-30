#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#include "sht30.h"
#include "m5device.h"

#define SHT30_I2C      I2C_NUM_1
#define SHT30_I2C_ADDR 0x44

#define SHT30_CRC_LEN 2
#define SHT30_CRC_POLYNOMIAL 0x31

static uint8_t sht30_check_crc(uint8_t *buf, uint8_t crc);

esp_err_t sht30_init(void)
{
  return m5device_init();
}

esp_err_t sht30_deinit(void)
{
  return m5device_deinit();
}

esp_err_t sht30_start_measurement(void)
{
  esp_err_t err = ESP_OK;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, 0x2C, true);
  i2c_master_write_byte(cmd, 0x10, true);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(SHT30_I2C, cmd, pdMS_TO_TICKS(3000));
  i2c_cmd_link_delete(cmd);
  return err;
}

esp_err_t sht30_wait_measurement(void)
{
  esp_err_t err = ESP_OK;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_READ, true);
  i2c_master_start(cmd);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(SHT30_I2C, cmd, pdMS_TO_TICKS(3000));
  i2c_cmd_link_delete(cmd);
  return err;
}

esp_err_t sht30_read_measured_values(uint16_t *temperature, uint16_t *humidity)
{
  esp_err_t err = ESP_OK;
  uint8_t code[2] = { 0x22, 0x36 };
  uint8_t crc[2];
  uint8_t temp[2];
  uint8_t hum[2];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, code[0], true);
  i2c_master_write_byte(cmd, code[1], true);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_READ, true);
  i2c_master_read_byte(cmd, temp, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, temp+1, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, crc, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, hum, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, hum+1, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, crc+1, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(SHT30_I2C, cmd, pdMS_TO_TICKS(3000));
  i2c_cmd_link_delete(cmd);

  if (!sht30_check_crc(temp, crc[0])) {
    ESP_LOGI("sht30", "temp %d(%x, %x), crc %d(%x), check result = %d", (uint8_t)((temp[0]<<8)+temp[1]), temp[0], temp[1], crc[0], crc[0], sht30_check_crc(temp, crc[0]));
    temp[0] = 0;
    temp[1] = 0;
    err = ESP_ERR_INVALID_CRC;
  }

  if (!sht30_check_crc(hum, crc[1])) {
    ESP_LOGI("sht30", "humdity %d(%x, %x), crc %d(%x), check result = %d", (uint8_t)((hum[0]<<8)+hum[1]), hum[0], hum[1], crc[1], crc[1], sht30_check_crc(hum, crc[1]));
    hum[0] = 0;
    hum[1] = 0;
    err = ESP_ERR_INVALID_CRC;
  }
  *temperature = (temp[0] << 8) | temp[1];
  *humidity = (hum[0] << 8) | hum[1];
  return err;
}

float sht30_calc_celsius(uint16_t temp)
{
  return -45.0F + 175.0F * (temp/(65536.0F-1));
}

float sht30_calc_relative_humidity(uint16_t hum)
{
  return 100.0F * hum / (65536.0F - 1);
}

esp_err_t sht30_heater(bool b)
{
  esp_err_t err = ESP_OK;
  uint8_t c[2] = { 0x30, 0x6d };
  if (!b) {
    c[1] = 0x66;
  }
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, c[0], true);
  i2c_master_write_byte(cmd, c[1], true);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(SHT30_I2C, cmd, pdMS_TO_TICKS(3000));
  i2c_cmd_link_delete(cmd);
  return err;
}

static uint8_t sht30_check_crc(uint8_t *buf, uint8_t crc)
{
  uint8_t v = 0xff;
  for (int i = 0; i < SHT30_CRC_LEN; i++) {
    v ^= *buf++;
    for (int j = 0; j < 8; j++) {
      if (v & 0x80) {
        v <<= 1;
        v ^= SHT30_CRC_POLYNOMIAL;
      } else {
        v <<= 1;
      }
    }
  }
  return v;
  /* ESP_LOGI("sht30", "crc should be 0x%x. calc = 0x%x", crc, v); */
  /* return (v ==  crc); */
}
