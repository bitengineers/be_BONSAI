#include <stdio.h>

#include "esp_log.h"

#include "wificlient.h"

static const char *TAG = "IoT_Plant";

void app_main(void)
{
  ESP_LOGI(TAG, "app_main: started.");

  wifi_client_init();
  wifi_client_wait_for_connected(pdMS_TO_TICKS(1000 * 60 * 60 * 1));


  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10 * 1000));
  }
}
