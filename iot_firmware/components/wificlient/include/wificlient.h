#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define WIFICLIENT_KEY_SSID      (char *)"SSID"
#define WIFICLIENT_KEY_PASSWORD  (char *)"PASSWORD"
#define WIFICLIENT_KEY_BSSID_SET (char *)"BSSID_SET"
#define WIFICLIENT_KEY_BSSID     (char *)"BSSID"

  typedef struct {
    // power save mode
    wifi_ps_type_t power_save;
  } wificlient_config_t;

  typedef struct wificlient_event {
    uint32_t event_id;
  } wificlient_event_t;

  esp_err_t wificlient_init(wificlient_config_t *config);
  esp_err_t wificlient_deinit(void);
  esp_err_t wificlient_deinit_with_check(void);
  esp_err_t wificlient_start(void);
  esp_err_t wificlient_stop(void);
  bool wificlient_is_connected(void);
  esp_err_t wificlient_wait_for_connected(TickType_t xTicksToWait);

  void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
  void ip_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);
  void smart_config_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);

#if defined(UNITY)
  extern nvs_handle_t s_wificlient_handle;
  extern esp_netif_t *sta_netif;
  extern wificlient_config_t *s_wificlient_config;
  extern uint8_t s_wificlient_has_credentials;
  extern wificlient_event_t event_log;
  extern SemaphoreHandle_t s_wc_sem;

  wificlient_event_t *wificlient_event_get_latest(void);

#endif // UNITY


#ifdef __cplusplus
}
#endif // __cplusplus
