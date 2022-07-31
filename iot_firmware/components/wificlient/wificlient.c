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

#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"

#include "wificlient.h"


#define WIFICELINT_EVQUEUE_NUM   (1)
#define WIFICELINT_EVQUEUE_TIMEOUT_TICK   (100)

static const char *TAG = "wificlient";

static void connect_task(void *parm);
static void wificlient_connect(void);
static void wificlient_set_bits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToSet );
static void wificlient_clear_bits( EventGroupHandle_t xEventGroup,
                                   const EventBits_t uxBitsToClear );
/* EventGroup and bits */
static EventGroupHandle_t s_wificlient_event_group;
static const int CONNECTED_BIT = BIT0;
static const int DONE_BIT = BIT1;

/* NVS handle for wifi client */
nvs_handle_t s_wificlient_handle = 0;

SemaphoreHandle_t s_wc_sem = NULL;
SemaphoreHandle_t s_wc_sem_bits = NULL;
QueueHandle_t s_wc_queue;
wificlient_event_t event_log = {
  .event_id = 0
};

/* Static variables for credentials */
uint8_t s_wificlient_has_credentials = 0;
static uint8_t s_wificlient_ssid[33] = { 0 };
static uint8_t *s_wificlient_password[65] = { 0 };
static uint8_t bssid_set = 0;
static uint8_t *s_wificlient_bssid[7] = { 0 };

/* WIFI interface */
#if defined(UNITY)
esp_netif_t *sta_netif = NULL;
#else
static esp_netif_t *sta_netif = NULL;
#endif // UNITY

/* wificlient configuration */
#if defined(UNITY)
wificlient_config_t *s_wificlient_config;
#else
static wificlient_config_t *s_wificlient_config;
#endif // UNITY


static uint8_t _wificlient_load_credentials()
{
  size_t required;
  // Check saved credentials
  // SSID
  nvs_get_str(s_wificlient_handle, WIFICLIENT_KEY_SSID, (char *)s_wificlient_ssid, &required);
  if (required != sizeof(s_wificlient_ssid)) {
    // error log
    ESP_LOGE(TAG, "Reading the SSDI failed");
  }
  // PASSWORD
  nvs_get_str(s_wificlient_handle, WIFICLIENT_KEY_PASSWORD, (char *)s_wificlient_password, &required);
  if (required != sizeof(s_wificlient_password)) {
    // error log
    ESP_LOGE(TAG, "Reading the PASSWORD failed");
  }
  // BSSID
  nvs_get_u8(s_wificlient_handle, WIFICLIENT_KEY_BSSID_SET, &bssid_set);
  if (bssid_set) {
    nvs_get_str(s_wificlient_handle, WIFICLIENT_KEY_BSSID, (char *)s_wificlient_bssid, &required);
    if (required != sizeof(s_wificlient_password)) {
      // error log
      ESP_LOGE(TAG, "Reading the PASSWORD failed");
    }
  }
  if (strlen((const char*)s_wificlient_ssid) > 0 && strlen((const char*)s_wificlient_password) > 0) {
    return 1;
  }

  return 0;
}

static void wificlient_connect(void)
{
  // Connect WIFI with saved credentials
  if (s_wificlient_has_credentials) {
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, s_wificlient_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, s_wificlient_password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = bssid_set;
    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, s_wificlient_bssid, sizeof(wifi_config.sta.bssid));
    }

    if (s_wificlient_config->power_save != WIFI_PS_NONE) {
      esp_wifi_set_ps(s_wificlient_config->power_save);
    }
    // ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
}

esp_err_t wificlient_init(wificlient_config_t *config)
{
  esp_err_t err;
  ESP_LOGI(TAG, "start initializing.");
  s_wificlient_config = config;

  if (s_wificlient_handle == 0) {
    err = nvs_open("wificlient", NVS_READWRITE, &s_wificlient_handle);
    if (err != ESP_OK) {
      s_wificlient_handle = 0;
    }
  }

  ESP_ERROR_CHECK(esp_netif_init());
  s_wificlient_event_group = xEventGroupCreate();
  err = esp_event_loop_create_default();
  switch(err) {
  case ESP_OK:
    // success
  case ESP_ERR_INVALID_STATE:
    // already created
    break;
  default:
    abort();
  }
  if (sta_netif == NULL) {
    sta_netif = esp_netif_create_default_wifi_sta();
  }
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  // ESP_ERROR_CHECK(esp_wifi_set_ps(s_wificlient_config->power_save));
  memset(s_wificlient_ssid, 0, sizeof(s_wificlient_ssid));
  memset(s_wificlient_password, 0, sizeof(s_wificlient_password));
  memset(s_wificlient_bssid, 0, sizeof(s_wificlient_bssid));
  s_wc_sem = xSemaphoreCreateBinary();
  s_wc_sem_bits = xSemaphoreCreateBinary();
  xSemaphoreGive(s_wc_sem_bits);
  s_wc_queue = xQueueGenericCreate(WIFICELINT_EVQUEUE_NUM, sizeof(uint16_t), queueOVERWRITE);

  // WIFI EVENT
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  // IP EVENT
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
    // Smart Config EVENT
  ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smart_config_event_handler, NULL));
  s_wificlient_has_credentials = _wificlient_load_credentials();

  ESP_LOGI(TAG, "initialized.");
  return ESP_OK;
}

esp_err_t wificlient_deinit(void)
{
  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_restore();
  if (s_wificlient_event_group != NULL) {
    wificlient_clear_bits(s_wificlient_event_group, CONNECTED_BIT|DONE_BIT);
  }
  vSemaphoreDelete(s_wc_sem);
  vSemaphoreDelete(s_wc_sem_bits);
  vQueueDelete(s_wc_queue);
  return ESP_OK;
}

esp_err_t wificlient_deinit_with_check(void)
{
  ESP_ERROR_CHECK(esp_wifi_disconnect());
  xEventGroupWaitBits(s_wificlient_event_group, CONNECTED_BIT|DONE_BIT,
                      true, true, 100);
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_restore());
  ESP_ERROR_CHECK(esp_wifi_deinit());
  if (s_wificlient_event_group != NULL) {
    wificlient_clear_bits(s_wificlient_event_group, CONNECTED_BIT|DONE_BIT);
    vEventGroupDelete(s_wificlient_event_group);
    s_wificlient_event_group = NULL;
  }
  ESP_ERROR_CHECK(esp_event_loop_delete_default());
  vSemaphoreDelete(s_wc_sem);
  vSemaphoreDelete(s_wc_sem_bits);
  vQueueDelete(s_wc_queue);
  return ESP_OK;
}

esp_err_t wificlient_start(void)
{
  // Start Tasks
  if (s_wc_sem == NULL) {
    s_wc_sem = xSemaphoreCreateBinary();
  }
  xTaskCreate(connect_task, "connect_task", 4096, NULL, 3, NULL);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  return ESP_OK;
}

esp_err_t wificlient_stop(void)
{
  if (s_wc_sem != NULL) {
    vSemaphoreDelete(s_wc_sem);
    s_wc_sem = NULL;
  }
  ESP_ERROR_CHECK(esp_wifi_stop());
  return ESP_OK;
}

bool wificlient_is_connected(void)
{
  if (xEventGroupGetBits(s_wificlient_event_group) && CONNECTED_BIT|DONE_BIT) {
    return true;
  }

  return false;
}

esp_err_t wificlient_wait_for_connected(TickType_t xTicksToWait)
{
  xEventGroupWaitBits(s_wificlient_event_group, CONNECTED_BIT|DONE_BIT,
                      false, true, xTicksToWait);
  if (xEventGroupGetBits(s_wificlient_event_group) && CONNECTED_BIT|DONE_BIT) {
    return ESP_OK;
  }
  return ESP_FAIL;
}

static void connect_task(void *parm)
{
  uint16_t retry;
  EventBits_t uxBits;
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_AIRKISS)); */
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS)); */
  /* ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2)); */

  do {
    if (s_wc_sem == NULL) {
      break;
    } else if (xSemaphoreTake(s_wc_sem, (TickType_t) 1000) != pdTRUE) {
      continue;
    }
    if (!s_wificlient_has_credentials) {
      ESP_LOGI(TAG, "connect_task: smart_config start");
      smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
      ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
      retry = 3;
      do {
        uxBits = xEventGroupWaitBits(s_wificlient_event_group,
                                     CONNECTED_BIT | DONE_BIT, false, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
          // ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & DONE_BIT) {
          // ESP_LOGI(TAG, "smartconfig over");
          esp_smartconfig_stop();
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
      } while (retry-- > 0);
    } else {
      ESP_LOGI(TAG, "connect task: start connecting");
      wificlient_connect();
      retry = 10;
      do {
        uxBits = xEventGroupWaitBits(s_wificlient_event_group,
                                     CONNECTED_BIT | DONE_BIT, false, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
          ESP_LOGI(TAG, "connect_task: wifi connected");
          esp_netif_dhcpc_stop(sta_netif);            
          esp_netif_dhcp_status_t dhcp_status;
          ESP_ERROR_CHECK(esp_netif_dhcpc_get_status(sta_netif, &dhcp_status));
          if (dhcp_status != ESP_NETIF_DHCP_STARTED) {
            ESP_LOGI(TAG, "connect_task: wifi connected, but DHCP have not started");
            esp_netif_dhcpc_start(sta_netif);            
          }
          wificlient_set_bits(s_wificlient_event_group, DONE_BIT);
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
      } while (retry-- > 0);
    }
  } while (s_wc_sem != NULL);

  vTaskDelete(NULL);
}

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  if (event_base != WIFI_EVENT) {
    return;
  }
  event_log.event_id = event_id;
  xQueueOverwrite(s_wc_queue, (const void * const)&event_log);
  switch(event_id) {
  case WIFI_EVENT_WIFI_READY:
    ESP_LOGI(TAG, "WIFI_EVENT: wifi_ready.");
    break;
  case WIFI_EVENT_SCAN_DONE:
    ESP_LOGI(TAG, "WIFI_EVENT: scan done.");
    break;
  case WIFI_EVENT_STA_START:
    ESP_LOGI(TAG, "WIFI_EVENT: sta started.");
    xSemaphoreGive(s_wc_sem);
    break;
  case WIFI_EVENT_STA_STOP:
    ESP_LOGI(TAG, "WIFI_EVENT: sta stoppped.");
    break;
  case WIFI_EVENT_STA_CONNECTED:
    ESP_LOGI(TAG, "WIFI_EVENT: sta connected.");
    break;
  case WIFI_EVENT_STA_DISCONNECTED:
    ESP_LOGI(TAG, "WIFI_EVENT: sta disconnected.");
    wificlient_clear_bits(s_wificlient_event_group, CONNECTED_BIT|DONE_BIT);
    if (s_wc_sem != NULL) {
      xSemaphoreGive(s_wc_sem);
    }
    break;
  case WIFI_EVENT_STA_BEACON_TIMEOUT:
    ESP_LOGI(TAG, "Station received beacon timeout event.");
    break;
  default:
    ESP_LOGI(TAG, "WIFI_EVENT: event_id = %d\n", event_id);
    return;
  }
}

void ip_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  if (event_base != IP_EVENT) {
    return;
  }
  switch(event_id) {
  case IP_EVENT_STA_GOT_IP:
    ESP_LOGI(TAG, "IP_EVENT: Got IP");
    wificlient_set_bits(s_wificlient_event_group, CONNECTED_BIT);
    break;
  case IP_EVENT_STA_LOST_IP:
    ESP_LOGI(TAG, "IP_EVENT: Lost IP");
    // wificlient_clear_bits(s_wificlient_event_group, CONNECTED_BIT);
    break;
  default:
    ESP_LOGI(TAG, "IP_EVENT: event_id = %d\n", event_id);
    return;
  }
}

void smart_config_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
  esp_err_t err;
  if (event_base != SC_EVENT) {
    return;
  }
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
    //bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = evt->bssid_set;
    if (wifi_config.sta.bssid_set == true) {
      memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
    }
    ESP_LOGI(TAG, "SSID:%s", evt->ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", evt->password);
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM)); // s_wificlient_config->power_save));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();

    // store credentials
    err = nvs_set_str(s_wificlient_handle, WIFICLIENT_KEY_SSID, (const char*)evt->ssid);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "Failed nvs_set ssid");
    }
    err = nvs_set_str(s_wificlient_handle, WIFICLIENT_KEY_PASSWORD, (const char*)evt->password);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "Failed nvs_set password");
    }
    err = nvs_set_u8(s_wificlient_handle, WIFICLIENT_KEY_BSSID_SET, evt->bssid_set);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "Failed nvs_set bssid_set");
    }
    err = nvs_set_str(s_wificlient_handle, WIFICLIENT_KEY_BSSID, (const char*)evt->bssid);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "Failed nvs_set bssid");
    }
    break;
  case SC_EVENT_SEND_ACK_DONE:
    ESP_LOGI(TAG, "SC_EVENT: SEND_ACK_DONE");
    wificlient_set_bits(s_wificlient_event_group, DONE_BIT);
    break;
  default:
    ESP_LOGI(TAG, "SC_EVENT: event_id = %d\n", event_id);
    return;
  }
}

wificlient_event_t *wificlient_event_get_latest(void)
{
  xQueueReceive(s_wc_queue, (void * const)&event_log, WIFICELINT_EVQUEUE_TIMEOUT_TICK);
  return &event_log;
}


static void wificlient_set_bits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToSet )
{
  while (1) {
    ESP_LOGI(TAG, "set_bits before");
    if (xSemaphoreTake(s_wc_sem_bits, 10)) {
      xEventGroupSetBits(xEventGroup, uxBitsToSet);
      xSemaphoreGive(s_wc_sem_bits);
      ESP_LOGI(TAG, "set_bits after");
      break;
    }
  }
}

static void wificlient_clear_bits( EventGroupHandle_t xEventGroup,
                                  const EventBits_t uxBitsToClear )
{
  while (1) {
    ESP_LOGI(TAG, "clear_bits before");
    if (xSemaphoreTake(s_wc_sem_bits, 10)) {
      xEventGroupClearBits(xEventGroup, uxBitsToClear);
      xSemaphoreGive(s_wc_sem_bits);
      ESP_LOGI(TAG, "clear_bits after");
      break;
    }
  }
}
