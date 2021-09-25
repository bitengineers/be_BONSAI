#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "axp192.h"
#include "wificlient.h"
#include "soilsensor.h"
#include "awsclient.h"

#include "main.h"


#define WAKE_UP_PIN ((gpio_num_t) 37)
#define JSON_BUFFER_MAX_LENGTH 255


static const char *TAG = "IoT_Plant";
static const uint64_t wakeup_time_sec_us = 3 * 60 * 1000 * 1000;

wificlient_config_t wc_config = {
  // .power_save = WIFI_PS_NONE,
  .power_save = WIFI_PS_MIN_MODEM,
  // .power_save = WIFI_PS_MAX_MODEM
};

awsclient_config_t awsconfig = {
  .shadow_params = {
    .pHost = CONFIG_AWS_IOT_MQTT_HOST,
    .port  = CONFIG_AWS_IOT_MQTT_PORT,
    .pClientCRT = (const char *)certificate_pem_crt_start,
    .pClientKey = (const char *)private_pem_key_start,
    .pRootCA    = (const char *)aws_root_ca_pem_start,
    .enableAutoReconnect = true,
    .disconnectHandler = NULL
  },
  .shadow_connect_params = {
    .pMyThingName = CONFIG_AWS_IOT_THING_NAME,
    .pMqttClientId = CONFIG_AWS_IOT_CLIENT_ID,
    .mqttClientIdLen = (uint16_t) strlen(CONFIG_AWS_IOT_CLIENT_ID),
    .deleteActionHandler = NULL,
  },
  .timeout_sec = 10,
};

char jsonDocumentBuffer[JSON_BUFFER_MAX_LENGTH];

static void app_pm_config(void);
static void goto_sleep(void);
static void wakeup_cause();

void app_main(void)
{
  esp_err_t err;
  ESP_LOGI(TAG, "app_main: started.");
  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  // Power Mgmt
  app_pm_config();

  // PMU
  axp192_init();
  axp192_chg_set_target_vol(AXP192_VOL_4_2);
  axp192_chg_set_current(AXP192_CHG_CUR_190);
  axp192_adc_batt_vol_en(true);
  axp192_adc_batt_cur_en(true);

  while (true) {
    // WIFI
    esp_err_t rtn;
    uint8_t retry = 0;
    wificlient_init(&wc_config);
    do {
      rtn = wificlient_wait_for_connected(pdMS_TO_TICKS(1000 * 3));
      retry++;
      if (retry > 10) {
        break;
      }
    } while (rtn != ESP_OK);

    // Update sensor values...
    // 1. Soil sensor
    //    Enable 5V output
    axp192_exten(true);
    soilsensor_init();
    //    Wait a few seconds, to acquire precise value
    vTaskDelay(pdMS_TO_TICKS(3000));
    uint16_t soil_value = soilsensor_get_value();
    if (!axp192_is_charging()) {
      axp192_exten(false);
    }
    ESP_LOGI(TAG, "adc output = %d\n", soil_value);

    // 2. Battery info
    uint16_t vol = axp192_batt_vol_get();
    uint16_t cur = axp192_batt_dischrg_cur_get();
    uint16_t chrg_cur = axp192_batt_chrg_cur_get();
    ESP_LOGI(TAG,
    "battery (voltage, current, charge_current) = (%d, %d, %d)\n",
             vol, cur, chrg_cur);

    // AWS
    awsclient_shadow_init(&awsconfig);
    // create json objects
    size_t jsonDocumentBufferSize = sizeof(jsonDocumentBuffer)/sizeof(char);
    aws_iot_shadow_init_json_document(jsonDocumentBuffer,
                                      jsonDocumentBufferSize);
    struct jsonStruct soil;
    soil.cb = NULL;
    soil.pData = &soil_value;
    soil.dataLength = sizeof(uint16_t);
    soil.pKey = "soil_value";
    soil.type = SHADOW_JSON_UINT16;

    struct jsonStruct batt_vol;
    batt_vol.pKey = "voltage";
    batt_vol.pData = &vol;
    batt_vol.dataLength = sizeof(uint16_t);
    batt_vol.type = SHADOW_JSON_UINT16;
    batt_vol.cb = NULL;
    struct jsonStruct batt_cur;
    batt_cur.pKey = "current";
    batt_cur.pData = &cur;
    batt_cur.dataLength = sizeof(uint16_t);
    batt_cur.type = SHADOW_JSON_UINT16;
    batt_cur.cb = NULL;
    struct jsonStruct batt_chrgcur;
    batt_chrgcur.pKey = "charge_current";
    batt_chrgcur.pData = &chrg_cur;
    batt_chrgcur.dataLength = sizeof(uint16_t);
    batt_chrgcur.type = SHADOW_JSON_UINT16;
    batt_chrgcur.cb = NULL;

    aws_iot_shadow_add_reported(jsonDocumentBuffer,
                                jsonDocumentBufferSize,
                                4, &soil, &batt_vol, &batt_cur, &batt_chrgcur);
    aws_iot_finalize_json_document(jsonDocumentBuffer,
                                   jsonDocumentBufferSize);
    ESP_LOGI(TAG, "json = %s", jsonDocumentBuffer);
    // AWS update shadow
    awsclient_shadow_update(&awsconfig, jsonDocumentBuffer, jsonDocumentBufferSize);
    ESP_LOGI(TAG, "awsclient_shadow_update returns %d\n", awsclient_err());
    if (awsclient_err() == NETWORK_SSL_WRITE_ERROR) {
      awsclient_shadow_init(&awsconfig);
      awsclient_shadow_update(&awsconfig, jsonDocumentBuffer, jsonDocumentBufferSize);
    } else if (awsclient_err() == NETWORK_ERR_NET_UNKNOWN_HOST) {
      wificlient_deinit();
      vTaskDelay(pdMS_TO_TICKS(1000));
      wificlient_init(&wc_config);
    }

    awsclient_shadow_deinit(&awsconfig);
    wificlient_deinit();
    // sleep
    goto_sleep();
    // esp_task_wdt_reset();
  }
}


static void app_pm_config(void)
{
#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
#if CONFIG_IDF_TARGET_ESP32
    esp_pm_config_esp32_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S2
    esp_pm_config_esp32s2_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C3
    esp_pm_config_esp32c3_t pm_config = {
#endif
            .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
            .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif // CONFIG_PM_ENABLE
}

static void goto_sleep(void)
{
  // sleep
  ESP_LOGI(TAG, "preparing sleep");
  //  wake from timer
  esp_sleep_enable_timer_wakeup(wakeup_time_sec_us);
  //  wake from gpio button
  /* esp_sleep_enable_gpio_wakeup(); */
  /* rtc_gpio_pulldown_dis(WAKE_UP_PIN); */
  /* rtc_gpio_pullup_en(WAKE_UP_PIN); */
  /* esp_sleep_enable_ext0_wakeup(WAKE_UP_PIN, 0); */
  //  wake from wifi
  // esp_sleep_enable_wifi_wakeup();
  ESP_LOGI(TAG, "entering sleep");
  // wait logging finished
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_light_sleep_start();
  // disable wake from timer
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  ESP_LOGI(TAG, "exiting sleep");
  // disable wake from gpio
  /* esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO); */
  /* rtc_gpio_deinit(WAKE_UP_PIN); */
  // disable wake from wifi
  // esp_sleep_disable_wifi_wakeup();

  wakeup_cause();
}

static void wakeup_cause()
{
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause) {
  case ESP_SLEEP_WAKEUP_TIMER:
    ESP_LOGI(TAG, "wake cause by timer");
    break;
  case ESP_SLEEP_WAKEUP_GPIO:
    ESP_LOGI(TAG, "wake cause by GPIO");
    break;
  case ESP_SLEEP_WAKEUP_WIFI:
    ESP_LOGI(TAG, "wake cause by WIFI");
    break;
  default:
    ESP_LOGI(TAG, "wake cause by ???");
    break;
  }
}
