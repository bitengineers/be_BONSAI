#pragma once

#include "esp_err.h"

#include "aws_iot_shadow_json.h"

#ifdef __cpluscplus
extern "C" {
#endif // __cplusplus

  typedef struct app_sensors_device {
    float bat_vol;
    float bat_cur;
    float bat_chrg_cur;
  } app_sensors_device_t;

  typedef struct app_sensors_data {
    float temperature;
    float humidity;
  } app_sensors_data_t;

  extern app_sensors_device_t dev;
  extern app_sensors_data_t env;
  extern app_sensors_data_t soil;
  extern uint16_t light;
  extern uint16_t water_level;
  extern int32_t weight;
  extern float weight_lsb;

  esp_err_t app_sensors_init(void);
  esp_err_t app_sensors_proc(void);
  // esp_err_t app_sensors_report_as_json(struct jsonStruct *json);

#ifdef __cplusplus
}
#endif // __cplusplus
