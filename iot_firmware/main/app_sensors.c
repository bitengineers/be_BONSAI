#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "nvs_flash.h"

#include "axp192.h"
#include "esp_pahub.h"
#include "esp_pbhub.h"
#include "soilsensor.h"
#include "sht30.h"
#include "hx711.h"

#include "app_sensors.h"

#define APP_SENSORS_TAG "app_sensors"

#define APP_SENSORS_HX711_KEY_LSB         (char*) "LSB"
#define APP_SENSORS_HX711_KEY_ZERO_OFFSET (char*) "Z_OFFSET"

#define PORT_A_SDA (GPIO_NUM_32)
#define PORT_A_SCL (GPIO_NUM_33)

#define APP_SENSORS_HX711_LSB_DEFAULT  (0.001f)

app_sensors_device_t dev;
app_sensors_data_t env;
app_sensors_data_t soil;
uint16_t water_level = 0;
uint16_t light = 0;
int32_t weight = 0;
float weight_lsb = APP_SENSORS_HX711_LSB_DEFAULT;

static uint32_t weight_initialized = 0;
static nvs_handle_t s_app_sensors_nvs_handle = 0;

#ifdef CONFIG_PORT_A_I2C
static esp_err_t app_sensors_i2c_init(void);
static esp_err_t app_sensors_i2c_deinit(void);
static esp_err_t app_sensors_proc_hub(void);
#endif // CONFIG_PORT_A_I2C

#ifdef CONFIG_PORT_A_EARTH_UNIT
static esp_err_t app_sensors_proc_earth_unit(void);
#endif // CONFIG_PORT_A_EARTH_UNIT

union conv32 {
  uint32_t ui32;
  float f;
};

esp_err_t app_sensors_init(void)
{
  esp_err_t err;
  err = nvs_open("app_sensors", NVS_READWRITE, &s_app_sensors_nvs_handle);
  if (err != ESP_OK) {
    s_app_sensors_nvs_handle = 0;
  }

  return err;
}

esp_err_t app_sensors_proc(void)
{
  // PMU
  axp192_init();
  axp192_chg_set_target_vol(AXP192_VOL_4_2);
  axp192_chg_set_current(AXP192_CHG_CUR_190);
  axp192_adc_batt_vol_en(true);
  axp192_adc_batt_cur_en(true);
  dev.bat_vol = axp192_batt_vol_get() * 1.1f / 1000.0f;
  dev.bat_cur = axp192_batt_dischrg_cur_get() * 0.5f / 1000.0f;
  dev.bat_chrg_cur = axp192_batt_chrg_cur_get() * 0.5f / 1000.0f;
  axp192_exten(true);
  ESP_LOGI(APP_SENSORS_TAG,
           "battery (voltage, current, charge_current) = (%0.2f, %0.2f, %0.2f)\n",
           dev.bat_vol, dev.bat_cur, dev.bat_chrg_cur);
  axp192_deinit();

#if defined(CONFIG_PORT_A_I2C)
  ESP_LOGI(APP_SENSORS_TAG, "init I2C");
  app_sensors_proc_hub();
#elif defined(CONFIG_PORT_A_EARTH_UNIT)
  ESP_LOGI(APP_SENSORS_TAG, "init earth unit");
  app_sensors_proc_earth_unit();
#else
  ESP_LOGI(APP_SENSORS_TAG, "no sensors");
#endif // CONFIG_PORT_A_I2C

  hx711_init();
  // first measurement to set gain.
  hx711_measure();
  if (!weight_initialized) {
    esp_err_t e;
    uint32_t offset = 0;
    uint32_t lsb = 0;
    union conv32 conv;
    // offset
    e = nvs_get_u32(s_app_sensors_nvs_handle, APP_SENSORS_HX711_KEY_ZERO_OFFSET, &offset);
    ESP_LOGI(APP_SENSORS_TAG, "nvs_get_u32 ZERO_OFFSET returns %d", e);
    if (e != ESP_OK) {
      ESP_LOGI(APP_SENSORS_TAG, "Calibrate HX711 Zero Offset\n");
      for (int i = 0; i < 10; i++) {
        offset += hx711_measure();
      }
      offset /= 10;
      e = nvs_set_u32(s_app_sensors_nvs_handle, APP_SENSORS_HX711_KEY_ZERO_OFFSET, offset);
      ESP_LOGI(APP_SENSORS_TAG, "nvs_set_u32 for ZERO_OFFSET returns %d", e);
    }
    ESP_LOGI(APP_SENSORS_TAG, "HX711 Zero Offset = %d", offset);
    hx711_set_zero_offset(offset);

    // weight scale lsb
    e = nvs_get_u32(s_app_sensors_nvs_handle, APP_SENSORS_HX711_KEY_LSB, &lsb);
    if (e != ESP_OK) {
      ESP_LOGI(APP_SENSORS_TAG, "Failed to load lsb\n");
#ifdef CONFIG_WEIGHT_SCALE_PER_BIT
      weight_lsb = atof(CONFIG_WEIGHT_SCALE_PER_BIT);
      ESP_LOGI(APP_SENSORS_TAG, "Used CONFIG_WEIGHT_SCALE_PER_BIT as weight_lsb: %02f", (float)weight_lsb);
      conv.f = weight_lsb;
      e = nvs_set_u32(s_app_sensors_nvs_handle, APP_SENSORS_HX711_KEY_LSB, conv.ui32);
      ESP_LOGI(APP_SENSORS_TAG, "nvs_set_u32 for LSB returns %d", e);
#else
      weight_lsb = APP_SENSORS_HX711_LSB_DEFAULT;
#endif // CONFIG_WEIGHT_SCALE_PER_BIT
    } else {
      if (lsb > 0) {
        conv.ui32 = lsb;
        weight_lsb = conv.f;
      }
    }
    weight_initialized = 1;
  }

  ESP_LOGI(APP_SENSORS_TAG, "Wait for HX711 READY...");
  uint32_t weight_sum = 0;
  for (int i = 0; i < 10; i++) {
    hx711_wait_for_ready();
    weight_sum += hx711_measure();
  }
  weight = weight_sum/10;
  if (weight > (pow(2, 23)-1)) {
    weight &= 0x7FFFFF;
    weight = -1 * (weight + 1);
  }

  hx711_deinit();
  ESP_LOGI(APP_SENSORS_TAG, "HX711 returns %d", weight);
  return ESP_OK;
}

/* esp_err_t app_sensors_report_as_json(struct jsonStruct *json) */
/* { */
/*   return ESP_OK; */
/* } */

#ifdef CONFIG_PORT_A_I2C
static esp_err_t app_sensors_i2c_init(void)
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
  }

  return err;
}

static esp_err_t app_sensors_i2c_deinit(void)
{
  return i2c_driver_delete(I2C_NUM_1);
}
#endif // CONFIG_PORT_A_I2C

static esp_err_t app_sensors_proc_hub(void)
{
  esp_err_t err = ESP_OK;

#ifdef CONFIG_PORT_A_I2C
  app_sensors_i2c_init();
#endif // CONFIG_PORT_A_I2C

#ifdef CONFIG_I2C_PORT_A_HAS_PAHUB
  // HUB Init
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
  err = pahub_ch(PAHUB_DISABLE_CH_ALL);
#endif // CONFIG_I2C_SHT30_FOR_SOIL_ON_CH1_ON_PAHUB_ON_PORT_A=y
#endif // CONFIG_I2C_PORT_A_HAS_PAHUB

#ifdef CONFIG_I2C_PORT_A_HAS_PBHUB
#ifdef CONFIG_I2C_PORT_A_HAS_PAHUB
  err = pahub_ch(PAHUB_ENABLE_CH5);
  ESP_LOGI(APP_SENSORS_TAG, "pahub_ch enable ch5 returns %d", err);
#endif // CONFIG_I2C_PORT_A_HAS_PAHUB
#ifdef CONFIG_I2C_PORT_A_HAS_LIGHTSENSOR_VIA_CH0_ON_PBHUB
  light = pbhub_analog_read(PBHUB_CH0);
#endif // CONFIG_I2C_PORT_A_HAS_LIGHTSENSOR_VIA_CH0_ON_PBHUB

#ifdef CONFIG_I2C_PORT_A_HAS_EARTH_SENSOR_VIA_CH1_ON_PBHUB
  water_level = pbhub_analog_read(PBHUB_CH1);
#endif // CONFIG_I2C_PORT_A_HAS_EARTH_SENSOR_VIA_CH1_ON_PBHUB

#endif // CONFIG_I2C_PORT_A_HAS_PBHUB

#ifdef CONFIG_PORT_A_I2C
  // HUB Deinit
  err = app_sensors_i2c_deinit();
#endif // CONFIG_PORT_A_I2C
  return err;

}

#ifdef CONFIG_PORT_A_EARTH_UNIT
static esp_err_t app_sensors_proc_earth_unit(void)
{
  soilsensor_init();
  water_level = soilsensor_get_value();
  return ESP_OK;
}
#endif // CONFIG_PORT_A_EARTH_UNIT
