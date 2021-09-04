#ifndef __WIFICLIENT_H__
#define __WIFICLIENT_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  typedef struct {
    // power save mode
    wifi_ps_type_t power_save;
  } wifi_client_config_t;

  esp_err_t wifi_client_init(wifi_client_config_t *config);
  esp_err_t wifi_client_wait_for_connected(TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __WIFICLIENT_H__
