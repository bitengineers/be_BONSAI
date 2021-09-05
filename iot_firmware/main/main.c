#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "axp192.h"
#include "wificlient.h"
#include "soilsensor.h"

#define WAKE_UP_PIN ((gpio_num_t) 37)

static const char *TAG = "IoT_Plant";
static const uint64_t wakeup_time_sec_us = 10 * 1000 * 1000;

wifi_client_config_t wc_config = {
  .power_save = WIFI_PS_MAX_MODEM
};

void app_main(void)
{
  ESP_LOGI(TAG, "app_main: started.");
  while (1) {
    // PMU
    axp192_init();
    axp192_exten(true);

    // WIFI
    memset(&wc_config, 0, sizeof(wifi_client_config_t));
    wifi_client_init(&wc_config);
    wifi_client_wait_for_connected(pdMS_TO_TICKS(1000 * 10));

    // Soil sensor
    soilsensor_init();
    vTaskDelay(pdMS_TO_TICKS(500));
    int value = soilsensor_get_value();
    axp192_exten(false);
    ESP_LOGI(TAG, "adc output = %d\n", value);

    soilsensor_deinit();
    wifi_client_deinit();
    axp192_deinit();

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
}
