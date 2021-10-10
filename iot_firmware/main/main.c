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
#include "esp_pahub.h"
#include "esp_pbhub.h"
#include "sht30.h"

#include "main.h"
#include "app_sensors.h"
#include "app_sleep.h"

#define WAKE_UP_PIN ((gpio_num_t) 37)
#define JSON_BUFFER_MAX_LENGTH 511


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
  .timeout_sec = 30,
};

char jsonDocumentBuffer[JSON_BUFFER_MAX_LENGTH];

static void app_pm_config(void);

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

    if (rtn != ESP_OK) {
      continue;
    }

    // process sensors
    app_sensors_proc();

    // AWS
    awsclient_shadow_init(&awsconfig);
    // create json objects
    size_t jsonDocumentBufferSize = sizeof(jsonDocumentBuffer)/sizeof(char);
    aws_iot_shadow_init_json_document(jsonDocumentBuffer,
                                      jsonDocumentBufferSize);

    char *client_id = CONFIG_AWS_IOT_CLIENT_ID;

    struct jsonStruct device;
    device.cb = NULL;
    device.pData = client_id;
    device.pKey = "client_id";
    device.dataLength = strlen(client_id);
    device.type = SHADOW_JSON_STRING;
    struct jsonStruct waterlevel;
    waterlevel.cb = NULL;
    waterlevel.pData = &water_level;
    waterlevel.dataLength = sizeof(uint16_t);
    waterlevel.pKey = "water_level";
    waterlevel.type = SHADOW_JSON_UINT16;
    struct jsonStruct env_light;
    env_light.cb = NULL;
    env_light.pData = &light;
    env_light.dataLength = sizeof(uint16_t);
    env_light.pKey = "env_light";
    env_light.type = SHADOW_JSON_UINT16;
    struct jsonStruct env_temp;
    env_temp.cb = NULL;
    env_temp.pData = &env.temperature;
    env_temp.dataLength = sizeof(float);
    env_temp.pKey = "env_temperature";
    env_temp.type = SHADOW_JSON_FLOAT;
    struct jsonStruct env_hum;
    env_hum.cb = NULL;
    env_hum.pData = &env.humidity;
    env_hum.dataLength = sizeof(float);
    env_hum.pKey = "env_humidity";
    env_hum.type = SHADOW_JSON_FLOAT;
    struct jsonStruct soil_temp;
    soil_temp.cb = NULL;
    soil_temp.pData = &soil.temperature;
    soil_temp.dataLength = sizeof(float);
    soil_temp.pKey = "soil_temperature";
    soil_temp.type = SHADOW_JSON_FLOAT;
    struct jsonStruct soil_hum;
    soil_hum.cb = NULL;
    soil_hum.pData = &soil.humidity;
    soil_hum.dataLength = sizeof(float);
    soil_hum.pKey = "soil_humidity";
    soil_hum.type = SHADOW_JSON_FLOAT;
    struct jsonStruct batt_vol;
    batt_vol.pKey = "voltage";
    batt_vol.pData = &dev.bat_vol;
    batt_vol.dataLength = sizeof(float);
    batt_vol.type = SHADOW_JSON_FLOAT;
    batt_vol.cb = NULL;
    struct jsonStruct batt_cur;
    batt_cur.pKey = "current";
    batt_cur.pData = &dev.bat_cur;
    batt_cur.dataLength = sizeof(float);
    batt_cur.type = SHADOW_JSON_FLOAT;
    batt_cur.cb = NULL;
    struct jsonStruct batt_chrgcur;
    batt_chrgcur.pKey = "charge_current";
    batt_chrgcur.pData = &dev.bat_chrg_cur;
    batt_chrgcur.dataLength = sizeof(float);
    batt_chrgcur.type = SHADOW_JSON_FLOAT;
    batt_chrgcur.cb = NULL;

    aws_iot_shadow_add_reported(jsonDocumentBuffer,
                                jsonDocumentBufferSize,
                                10, &device, &env_temp, &env_hum, &env_light,
                                &soil_temp, &soil_hum,
                                &batt_vol, &batt_cur, &batt_chrgcur,
                                &waterlevel);
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

    // before sleep
    app_before_sleep();
    // sleep
    app_goto_sleep();
    // after wakeup
    app_after_wakeup();
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

