#include "sdkconfig.h"

#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "main.h"
#include "app_sleep.h"

#define WAKE_UP_PIN ((gpio_num_t) 37)

#ifdef CONFIG_SLEEP_TIMER_TIMEOUT
static const uint64_t s_wakeup_time_sec_us = CONFIG_SLEEP_TIMER_TIMEOUT;
#else
static const uint64_t s_wakeup_time_sec_us = 10 * 60 * 1000 * 1000;
#endif // CONFIG_SLEEP_TIMER_TIMEOUT


#ifdef CONFIG_M5STACK_CORE2
static void app_before_sleep_core2(void);
static void app_after_wakeup_core2(void);
#endif // CONFIG_M5STACK_CORE2

#ifdef CONFIG_M5STICK_C_PLUS
static void app_before_sleep_stickcplus(void);
static void app_after_wakeup_stickcplus(void);
#endif // CONFIG_M5STICK_C_PLUS

static void app_log_wakeup_cause(void);

void app_before_sleep(void)
{
  //  wake from timer
  esp_sleep_enable_timer_wakeup(s_wakeup_time_sec_us);
#if defined(CONFIG_M5STICK_C_PLUS)
  app_before_sleep_stickcplus();
#elif defined(CONFIG_M5STACK_CORE2)
  app_before_sleep_core2();
#endif // CONFIG_M5STICK_C_PLUS
}

void app_goto_sleep(void)
{
  ESP_LOGI(TAG, "entering sleep");
  // wait logging finished
  vTaskDelay(pdMS_TO_TICKS(100));

  // sleep
#if defined(CONFIG_SLEEP_TYPE_LIGHT)
  esp_light_sleep_start();
#elif defined(CONFIG_SLEEP_TYPE_DEEP)
  esp_light_deep_start();
#endif // CONFIG_SLEEP_TYPE_LIGHT
}

void app_after_wakeup(void)
{
  // disable wake from timer
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  ESP_LOGI(TAG, "exiting sleep");

#if defined(CONFIG_M5STICK_C_PLUS)
  app_after_wakeup_stickcplus();
#elif defined(CONFIG_M5STACK_CORE2)
  app_after_wakeup_core2();
#endif // CONFIG_M5STICK_C_PLUS

  app_log_wakeup_cause();
}


#ifdef CONFIG_M5STACK_CORE2

static void app_before_sleep_core2(void)
{
}

static void app_after_wakeup_core2(void)
{
}

#endif // CONFIG_M5STACK_CORE2

#ifdef CONFIG_M5STICK_C_PLUS

static void app_before_sleep_stickcplus(void)
{
  //  wake from gpio button
  esp_sleep_enable_gpio_wakeup();
  rtc_gpio_pulldown_dis(WAKE_UP_PIN);
  rtc_gpio_pullup_en(WAKE_UP_PIN);
  esp_sleep_enable_ext0_wakeup(WAKE_UP_PIN, 0);
}

static void app_after_wakeup_stickcplus(void)
{
  // disable wake from gpio
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  rtc_gpio_deinit(WAKE_UP_PIN);
}

#endif // CONFIG_M5STICK_C_PLUS

static void app_log_wakeup_cause()
{
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause) {
  case ESP_SLEEP_WAKEUP_TIMER:
    ESP_LOGI(TAG, "wake cause by timer");
    break;
  case ESP_SLEEP_WAKEUP_GPIO:
    ESP_LOGI(TAG, "wake cause by GPIO");
    break;
  case ESP_SLEEP_WAKEUP_WIFI:
    ESP_LOGI(TAG, "wake cause by WIFI");
    break;
  default:
    ESP_LOGI(TAG, "wake cause by ???");
    break;
  }
}
