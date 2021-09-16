#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_log.h"
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

wifi_client_config_t wc_config = {
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
static int s_main_aws_initialized = 0;

static void goto_sleep(void);


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

  // PMU
  axp192_init();

  while (1) {
    esp_err_t rtn;
    uint8_t retry = 0;
    // WIFI
    wifi_client_init(&wc_config);
    do {
      rtn = wifi_client_wait_for_connected(pdMS_TO_TICKS(1000 * 3));
      retry++;
      if (retry > 10) {
        break;
      }
    } while (rtn != ESP_OK);

    if (rtn == ESP_OK) {
      if (!s_main_aws_initialized) {
        // AWS
        awsclient_shadow_init(&awsconfig);
        s_main_aws_initialized = 1;
      }

      // Update sensor values...
      // 1. Soil sensor
      //    Enable 5V output
      axp192_exten(true);
      soilsensor_init();
      //    Wait a second, to acquire precise value
      vTaskDelay(pdMS_TO_TICKS(3000));
      uint16_t soil_value = soilsensor_get_value();
      axp192_exten(false);
      ESP_LOGI(TAG, "adc output = %d\n", soil_value);

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

      aws_iot_shadow_add_reported(jsonDocumentBuffer,
                                  jsonDocumentBufferSize,
                                  1, &soil);
      aws_iot_finalize_json_document(jsonDocumentBuffer,
                                     jsonDocumentBufferSize);
      ESP_LOGI(TAG, "json = %s", jsonDocumentBuffer);
      // AWS update shadow
      awsclient_shadow_update(&awsconfig, jsonDocumentBuffer, jsonDocumentBufferSize);
    }

    wifi_client_deinit();
    // sleep
    goto_sleep();
  }
}

static void goto_sleep(void) {
  // sleep
  ESP_LOGI(TAG, "preparing sleep");
  //  wake from timer
  esp_sleep_enable_timer_wakeup(wakeup_time_sec_us);
  //  wake from gpio button
  esp_sleep_enable_gpio_wakeup();
  rtc_gpio_pulldown_dis(WAKE_UP_PIN);
  rtc_gpio_pullup_en(WAKE_UP_PIN);
  esp_sleep_enable_ext0_wakeup(WAKE_UP_PIN, 0);
  ESP_LOGI(TAG, "entering sleep");
  // wait logging finished
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_light_sleep_start();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  ESP_LOGI(TAG, "exiting sleep");
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  rtc_gpio_deinit(WAKE_UP_PIN);
}
