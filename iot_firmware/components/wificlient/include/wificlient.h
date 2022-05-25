#pragma once

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
  } wificlient_config_t;

  esp_err_t wificlient_init(wificlient_config_t *config);
  esp_err_t wificlient_deinit(void);
  esp_err_t wificlient_deinit_with_check(void);
  esp_err_t wificlient_wait_for_connected(TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif // __cplusplus
