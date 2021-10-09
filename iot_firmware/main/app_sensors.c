#include <stdint.h>

#include "sdkconfig.h"

#include "esp32_axp192.h"
#include "esp_pahub.h"
#include "esp_pbhub.h"

#include "app_sensors.h"

#define APP_SENSORS_TAG "app_sensors"

#define PORT_A_SDA (GPIO_NUM_32)
#define PORT_A_SCL (GPIO_NUM_33)

app_sensors_device_t dev;
app_sensors_data_t env;
app_sensors_data_t soil;

static esp_err_t app_sensors_proc_hub(void);
static esp_err_t app_sensors_proc_earth_unit(void);


esp_err_t app_sensors_proc(void)
{
  esp_err_t err;
  // PMU
  axp192_init();
  axp192_chg_set_target_vol(AXP192_VOL_4_2);
  axp192_chg_set_current(AXP192_CHG_CUR_190);
  axp192_adc_batt_vol_en(true);
  axp192_adc_batt_cur_en(true);
  dev.bat_vol = axp192_batt_vol_get();
  dev.bat_cur = axp192_batt_dischrg_cur_get();
  dev.bat_chrg_cur = axp192_batt_chrg_cur_get();
  axp192_exten(true);
  ESP_LOGI(APP_SENSORS_TAG,
           "battery (voltage, current, charge_current) = (%d, %d, %d)\n",
           dev.bat_vol, dev.bat_cur, dev.bat_chrg_cur);
  axp192_deinit();

#if defined(CONFIG_PORT_A_I2C)
  app_sensors_proc_hub();
#else // CONFIG_PORT_A_EARTH_UNIT
  app_sensors_proc_earth_unit();
#endif // CONFIG_PORT_A_I2C

  return ESP_OK;
}

esp_err_t app_sensors_report_as_json(struct jsonStruct *json)
{
  return ESP_OK;
}

static esp_err_t app_sensors_proc_hub(void)
{

#if CONFIG_I2C_PULLUP_ENABLE
#define APP_SENSORS_PULLUP GPIO_PULLUP_ENABLE
#else
#define APP_SENSORS_PULLUP GPIO_PULLUP_DISABLE
#endif // CONFIG_I2C_PULLUP_ENABLE
  esp_err_t err;
  i2c_config_t i2c_config = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PORT_A_SDA,
    .scl_io_num = PORT_A_SCL,
    .sda_pullup_en = APP_SENSORS_PULLUP,
    .scl_pullup_en = APP_SENSORS_PULLUP,
    .master.clk_speed = CONFIG_I2C_BAUDRATE
  };
  err = i2c_param_config(I2C_NUM_1, &i2c_config);
  if (err != ESP_OK) {
    ESP_LOGI(APP_SENSORS_TAG, "i2c_param_config returns %d\n", err);
    return err;
  }
  err = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGI(APP_SENSORS_TAG, "i2c_driver_install returns %d\n", err);
    return err;
  }
  err = i2c_set_timeout(I2C_NUM_1, CONFIG_I2C_TIMEOUT);
  if (err != ESP_OK) {
    ESP_LOGI(APP_SENSORS_TAG, "i2c_set_timeout returns %d\n", err);
    return err;
  }

#ifdef CONFIG_PORT_A_I2C_HAS_PAHUB
  // HUB Init
  ESP_ERROR_CHECK(pahub_init());
  vTaskDelay(pdMS_TO_TICKS(1000));
  err = pahub_ch(PAHUB_DISABLE_CH_ALL);
  ESP_LOGI(APP_SENSORS_TAG, "pahub_ch disable ALL returns %d", err);

#ifdef CONFIG_I2C_SHT30_FOR_ENV_ON_CH0_ON_PAHUB_ON_PORT_A
  err = pahub_ch(PAHUB_ENABLE_CH0);
  ESP_LOGI(APP_SENSORS_TAG, "pahub_ch enable ch0 returns %d", err);
  uint16_t temp_raw = 0;
  uint16_t humidity_raw = 0;
  sht30_read_measured_values(&temp_raw, &humidity_raw);
  env.temperature = sht30_calc_celsius(temp_raw);
  env.humidity = sht30_calc_relative_humidity(humidity_raw);
  ESP_LOGI(APP_SENSORS_TAG, "temperature = %0.2f, humidity = %0.2f", env.temperature, env.humidity);
  err = pahub_ch(PAHUB_DISABLE_CH_ALL);
#endif // CONFIG_I2C_SHT30_FOR_ENV_ON_CH0_ON_PAHUB_ON_PORT_A

#ifdef CONFIG_I2C_SHT30_FOR_SOIL_ON_CH1_ON_PAHUB_ON_PORT_A
  err = pahub_ch(PAHUB_ENABLE_CH1);
  ESP_LOGI(APP_SENSORS_TAG, "pahub_ch enable ch1 returns %d", err);
  uint16_t soil_temp_raw = 0;
  uint16_t soil_humidity_raw = 0;
  sht30_read_measured_values(&soil_temp_raw, &soil_humidity_raw);
  soil.temperature = sht30_calc_celsius(soil_temp_raw);
  soil.humidity = sht30_calc_relative_humidity(soil_humidity_raw);
  ESP_LOGI(APP_SENSORS_TAG, "soil_temperature = %0.2f, soil_humidity = %0.2f", soil.temperature, soil.humidity);
#endif // CONFIG_I2C_SHT30_FOR_SOIL_ON_CH1_ON_PAHUB_ON_PORT_A=y

  // HUB Deinit
  pahub_deinit();
#endif // CONFIG_PORT_A_I2C_HAS_PAHUB  
}

static esp_err_t app_sensors_proc_earth_unit(void)
{
  soilsensor_init();
  return ESP_OK;
}
