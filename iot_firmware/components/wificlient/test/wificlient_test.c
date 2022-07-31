#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "unity.h"

#include "wificlient.h"


wificlient_config_t wc_config = {
  // .power_save = WIFI_PS_NONE,
  .power_save = WIFI_PS_MIN_MODEM,
  // .power_save = WIFI_PS_MAX_MODEM
};


TEST_CASE("wifi event handler", "[success]")
{
  nvs_flash_init();
  wificlient_init(&wc_config);
  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_WIFI_READY, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_WIFI_READY);

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_SCAN_DONE, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_SCAN_DONE);

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_START, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_STA_START);
  TEST_ASSERT_EQUAL(uxSemaphoreGetCount(s_wc_sem), 1);
  TEST_ASSERT_TRUE(xSemaphoreTake(s_wc_sem, (TickType_t)10));

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_STA_CONNECTED);
  TEST_ASSERT_EQUAL(uxSemaphoreGetCount(s_wc_sem), 0);

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_STA_DISCONNECTED);
  TEST_ASSERT_EQUAL(uxSemaphoreGetCount(s_wc_sem), 1);
  TEST_ASSERT_TRUE(xSemaphoreTake(s_wc_sem, (TickType_t)10));

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_STOP, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_STA_STOP);
  TEST_ASSERT_EQUAL(uxSemaphoreGetCount(s_wc_sem), 0);

  wifi_event_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_BEACON_TIMEOUT, NULL);
  TEST_ASSERT_EQUAL(event_log.event_id, WIFI_EVENT_STA_BEACON_TIMEOUT);
  TEST_ASSERT_EQUAL(uxSemaphoreGetCount(s_wc_sem), 0);
}

TEST_CASE("wifi event handle DISCONNECTED", "[failure]")
{
  uint16_t retry = 100;
  nvs_flash_init();

  nvs_set_str(s_wificlient_handle, WIFICLIENT_KEY_SSID, CONFIG_WIFICLIENT_TEST_SSID);
  nvs_set_str(s_wificlient_handle, WIFICLIENT_KEY_PASSWORD, CONFIG_WIFICLIENT_TEST_PASSWORD);

  wificlient_init(&wc_config);
  TEST_ASSERT_TRUE(s_wificlient_has_credentials);
  wificlient_start();
  while (retry-- > 0) {
    if (wificlient_is_connected()) {
      ESP_LOGI("TEST", "Free Heap Size %d", xPortGetFreeHeapSize());
      // TODO Reproduce disconnection to verify to be reconnectable.
      // esp_wifi_disconnect();
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  wificlient_stop();
  nvs_flash_erase();
}
