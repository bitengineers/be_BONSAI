/**
 *
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#include "stickcplus.h"

#define TAG "STICKC_PLUS"

bool stickcplus_is_initialized = false;
SemaphoreHandle_t xSemaphore = NULL;

/**
 * M5StickC+ initialization
 */
esp_err_t stickcplus_init(void)
{
  if (! stickcplus_is_initialized) {
    if (xSemaphore == NULL) {
      vSemaphoreCreateBinary(xSemaphore);
    }

#ifdef CONFIG_M5STICKC_PLUS_I2C
    ESP_ERROR_CHECK(stickcplus_i2c_init());
#endif // CONFIG_M5STICKC_PLUS_I2C

    if (xSemaphoreTake( xSemaphore, (TickType_t) 10) == pdTRUE) {
      stickcplus_is_initialized = true;
      xSemaphoreGive( xSemaphore );
    } else {
      ESP_LOGI(TAG, "In Initialization, semaphoreTake failed.");
      return ESP_ERR_TIMEOUT;
    }
  }

  return ESP_OK;
}

esp_err_t stickcplus_deinit(void)
{
  if (stickcplus_is_initialized) {
    if (xSemaphoreTake( xSemaphore, (TickType_t) 10) == pdTRUE) {
      stickcplus_is_initialized = false;
      xSemaphoreGive( xSemaphore );
    } else {
      ESP_LOGI(TAG, "In Deinitialization, semaphoreTake failed.");
      return ESP_ERR_TIMEOUT;
    }
  }

  return ESP_OK;
}

esp_err_t stickcplus_i2c_init(void)
{
  int i2c_port = I2C_NUM_0;
  gpio_pullup_t pullup = GPIO_PULLUP_DISABLE;
  i2c_mode_t mode = I2C_MODE_MASTER;
  uint32_t clk = 0;
  

#ifdef CONFIG_M5STICKC_PLUS_I2C_NUM_1
  i2c_port = I2C_NUM_1;
#endif // CONFIG_M5STICKC_PLUS_I2C_NUM_1

#ifdef CONFIG_M5STICKC_PLUS_PULLUP_ENABLE
  pullup = GPIO_PULLUP_ENABLE;
#endif // CONFIG_M5STICKC_PLUS_PULLUP_ENABLE

#ifdef CONFIG_M5STICKC_PLUS_I2C_SLAVE
  mode = I2C_MODE_SLAVE;
#endif // CONFIG_M5STICKC_PLUS_I2C_SLAVE

#ifdef CONFIG_M5STICKC_PLUS_I2C_CLK
  clk = (int)(CONFIG_M5STICKC_PLUS_I2C_CLK);
#endif // CONFIG_M5STICKC_PLUS_I2C_CLK

  i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = M5STICKC_PLUS_SDA,
    .scl_io_num = M5STICKC_PLUS_SCL,
    .sda_pullup_en = pullup,
    .scl_pullup_en = pullup,
    .master.clk_speed = clk
  };
  ESP_ERROR_CHECK(i2c_param_config(i2c_port, &i2c_config));
  ESP_ERROR_CHECK(i2c_driver_install(i2c_port, mode, 0, 0, 0));

  return ESP_OK;
}
