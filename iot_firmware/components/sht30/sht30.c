#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "sht30.h"

#define SHT30_I2C      I2C_NUM_1
#define SHT30_I2C_ADDR 0x44

#define SHT30_CRC_LEN 2
#define SHT30_CRC_POLYNOMIAL 0x31

static uint8_t sht30_check_crc(uint8_t *buf, uint8_t crc);

esp_err_t sht30_init(void)
{
  return ESP_OK;
}

esp_err_t sht30_deinit(void)
{
  return ESP_OK;
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
  return err;
}

esp_err_t sht30_read_measured_values(uint16_t *temperature, uint16_t *humidity)
{
  esp_err_t err = ESP_OK;
  uint8_t crc[2];
  uint8_t temp[2];
  uint8_t hum[2];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (SHT30_I2C_ADDR<<1)|I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, 0x24, true);
  i2c_master_write_byte(cmd, 0x16, true);
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

  ESP_LOGI("sht30", "temp %d(%x, %x), crc %d(%x), check result = %d", (uint8_t)((temp[0]<<8)+temp[1]), temp[0], temp[1], crc[0], crc[0], sht30_check_crc(temp, crc[0]));
  
  ESP_LOGI("sht30", "humdity %d(%x, %x), crc %d(%x), check result = %d", (uint8_t)((hum[0]<<8)+hum[1]), hum[0], hum[1], crc[1], crc[1], sht30_check_crc(hum, crc[1]));
  *temperature = (temp[0] << 8) | temp[1];
  *humidity = (hum[0] << 8) | hum[1];
  return err;
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
  i2c_master_write_byte(cmd, (uint8_t*)c, true);
  i2c_master_write_byte(cmd, (uint8_t*)(c+1), true);  
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(SHT30_I2C, cmd, pdMS_TO_TICKS(3000));
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
