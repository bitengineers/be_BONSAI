#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "esp_log.h"

#include "wificlient.h"


#define WIFI_CLIENT_KEY_SSID      (char *)"SSID"
#define WIFI_CLIENT_KEY_PASSWORD  (char *)"PASSWORD"
#define WIFI_CLIENT_KEY_BSSID_SET (char *)"BSSID_SET"
#define WIFI_CLIENT_KEY_BSSID     (char *)"BSSID"


static EventGroupHandle_t s_wifi_client_event_group;
static const char *TAG = "wificlient";
static void smartconfig_task(void *parm);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static void smart_config_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static nvs_handle_t s_wifi_client_handle;
static uint8_t s_wifi_client_ssid[33] = { 0 };
static uint8_t *s_wifi_client_password[65] = { 0 };
static uint8_t *s_wifi_client_bssid[7] = { 0 };
static uint8_t s_wifi_client_need_sc = 1;
static wifi_client_config_t *s_wifi_client_config;

esp_err_t wifi_client_init(wifi_client_config_t *config)
{
  esp_err_t err;
  size_t required;
  ESP_LOGI(TAG, "start initializing.");
  s_wifi_client_config = config;
  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  err = nvs_open("wifi_client", NVS_READWRITE, &s_wifi_client_handle);
  if (err != ESP_OK) {
    s_wifi_client_handle = 0;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  s_wifi_client_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  memset(s_wifi_client_ssid, 0, sizeof(s_wifi_client_ssid));
  memset(s_wifi_client_password, 0, sizeof(s_wifi_client_password));
  memset(s_wifi_client_bssid, 0, sizeof(s_wifi_client_bssid));

  // Check saved credentials
  // SSID
  nvs_get_str(s_wifi_client_handle, WIFI_CLIENT_KEY_SSID, (char *)s_wifi_client_ssid, &required);
  if (required != sizeof(s_wifi_client_ssid)) {
    // error log
  }
  // PASSWORD
  nvs_get_str(s_wifi_client_handle, WIFI_CLIENT_KEY_PASSWORD, (char *)s_wifi_client_password, &required);
  if (required != sizeof(s_wifi_client_password)) {
    // error log
  }
  // BSSID
  uint8_t bssid_set = 0;
  nvs_get_u8(s_wifi_client_handle, WIFI_CLIENT_KEY_BSSID_SET, &bssid_set);
  if (bssid_set) {
    nvs_get_str(s_wifi_client_handle, WIFI_CLIENT_KEY_BSSID, (char *)s_wifi_client_bssid, &required);
    if (required != sizeof(s_wifi_client_password)) {
      // error log
    }
  }
  // Connect WIFI with saved credentials
  if (strlen((const char*)s_wifi_client_ssid) > 0 && strlen((const char*)s_wifi_client_password) > 0) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, s_wifi_client_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, s_wifi_client_password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = bssid_set;
    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, s_wifi_client_bssid, sizeof(wifi_config.sta.bssid));
    }
    if (config->power_save != WIFI_PS_NONE) {
      esp_wifi_set_ps(config->power_save);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    s_wifi_client_need_sc = 0;
  } else {

    // WIFI EVENT
    ESP_ERROR_CHECK(
      esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // IP EVENT
    ESP_ERROR_CHECK(
      esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    // Smart Config EVENT
    ESP_ERROR_CHECK(
      esp_event_handler_register(
        SC_EVENT, ESP_EVENT_ANY_ID, &smart_config_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
  }

  ESP_LOGI(TAG, "initialized.");
  return ESP_OK;
}

esp_err_t wifi_client_wait_for_connected(TickType_t xTicksToWait)
{
  xEventGroupWaitBits(s_wifi_client_event_group, CONNECTED_BIT,
                      false, true, xTicksToWait);
  if (xEventGroupGetBits(s_wifi_client_event_group) && CONNECTED_BIT) {
    return ESP_OK;
  }
  return ESP_OK;
}

static void smartconfig_task(void *parm)
{
  EventBits_t uxBits;
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_AIRKISS)); */
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS)); */
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2)); */
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  if (s_wifi_client_need_sc) {
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
  }
  while (1) {
    uxBits = xEventGroupWaitBits(
      s_wifi_client_event_group,
      CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
    if(uxBits & CONNECTED_BIT) {
      ESP_LOGI(TAG, "WiFi Connected to ap");
    }
    if(uxBits & ESPTOUCH_DONE_BIT) {
      ESP_LOGI(TAG, "smartconfig over");
      if (s_wifi_client_need_sc) {
        esp_smartconfig_stop();
      }
      vTaskDelete(NULL);
    }
  }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT) {
    switch(event_id) {
    case WIFI_EVENT_WIFI_READY:
      ESP_LOGI(TAG, "WIFI_EVENT: wifi_ready.");
      break;
    case WIFI_EVENT_SCAN_DONE:
      ESP_LOGI(TAG, "WIFI_EVENT: scan done.");
      break;
    case WIFI_EVENT_STA_START:
      xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
      break;
    case WIFI_EVENT_STA_STOP:
      ESP_LOGI(TAG, "WIFI_EVENT: sta stoppped.");
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "WIFI_EVENT: sta connected.");
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "WIFI_EVENT: sta disconnected.");
      esp_wifi_connect();
      xEventGroupClearBits(s_wifi_client_event_group, CONNECTED_BIT);
      break;
    case WIFI_EVENT_STA_BEACON_TIMEOUT:
      ESP_LOGI(TAG, "Station received beacon timeout event.");
      break;
    default:
      ESP_LOGI(TAG, "WIFI_EVENT: event_id = %d\n", event_id);
      return;
    }
  }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  if (event_base == IP_EVENT) {
    switch(event_id) {
    case IP_EVENT_STA_GOT_IP:
      xEventGroupSetBits(s_wifi_client_event_group, CONNECTED_BIT);
      break;
    default:
      ESP_LOGI(TAG, "IP_EVENT: event_id = %d\n", event_id);
      return;
    }
  }
}

static void smart_config_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  esp_err_t err;
  if (event_base == SC_EVENT) {
    switch(event_id) {
    case SC_EVENT_SCAN_DONE:
      ESP_LOGI(TAG, "Scan done");
      break;
    case SC_EVENT_FOUND_CHANNEL:
      ESP_LOGI(TAG, "Found channel");
      break;
    case SC_EVENT_GOT_SSID_PSWD:
      ESP_LOGI(TAG, "Got SSID and password");
      smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
      wifi_config_t wifi_config;
      memset(&wifi_config, 0, sizeof(wifi_config_t));
      memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
      memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
      wifi_config.sta.bssid_set = evt->bssid_set;
      if (wifi_config.sta.bssid_set == true) {
        memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
      }
      if (s_wifi_client_config->power_save != WIFI_PS_NONE) {
        ESP_ERROR_CHECK(esp_wifi_set_ps(s_wifi_client_config->power_save));
      }

      ESP_LOGI(TAG, "SSID:%s", evt->ssid);
      ESP_LOGI(TAG, "PASSWORD:%s", evt->password);
      ESP_ERROR_CHECK(esp_wifi_disconnect());
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      esp_wifi_connect();

      err = nvs_set_str(s_wifi_client_handle, WIFI_CLIENT_KEY_SSID, (const char*)evt->ssid);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed nvs_set ssid");
      }
      err = nvs_set_str(s_wifi_client_handle, WIFI_CLIENT_KEY_PASSWORD, (const char*)evt->password);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed nvs_set password");
      }
      err = nvs_set_u8(s_wifi_client_handle, WIFI_CLIENT_KEY_BSSID_SET, evt->bssid_set);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed nvs_set bssid_set");
      }
      err = nvs_set_str(s_wifi_client_handle, WIFI_CLIENT_KEY_BSSID, (const char*)evt->bssid);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed nvs_set bssid");
      }
      break;
    case SC_EVENT_SEND_ACK_DONE:
      xEventGroupSetBits(s_wifi_client_event_group, ESPTOUCH_DONE_BIT);
      break;
    default:
      ESP_LOGI(TAG, "SC_EVENT: event_id = %d\n", event_id);
      return;
    }
  }
}
