#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "wificlient.h"
#include "soilsensor.h"

static const char *TAG = "IoT_Plant";

wifi_client_config_t wc_config = {
  // .power_save = WIFI_PS_NONE
  .power_save = WIFI_PS_MAX_MODEM
};

void app_main(void)
{
  ESP_LOGI(TAG, "app_main: started.");
  memset(&wc_config, 0, sizeof(wifi_client_config_t));
  wifi_client_init(&wc_config);
  wifi_client_wait_for_connected(pdMS_TO_TICKS(1000 * 10));

  soilsensor_init();
  vTaskDelay(pdMS_TO_TICKS(1000));
  while (1) {
    int value = soilsensor_get_value();
    ESP_LOGI(TAG, "adc output = %d\n", value);
    vTaskDelay(pdMS_TO_TICKS(10 * 1000));
  }
}
