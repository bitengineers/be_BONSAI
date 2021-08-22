#ifndef __WIFICLIENT_H__
#define __WIFICLIENT_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  esp_err_t wifi_client_init(void);
  esp_err_t wifi_client_wait_for_connected(TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __WIFICLIENT_H__
