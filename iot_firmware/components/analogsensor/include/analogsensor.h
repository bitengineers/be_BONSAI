#ifndef __ANALOGSENSOR_H__
#define __ANALOGSENSOR_H__

#include <stdbool.h>

#include <driver/adc_common.h>
#include <hal/adc_types.h>


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  typedef struct {
    adc_bits_width_t bitwidth;
    adc1_channel_t channel;
    adc_atten_t atten;
  } analogsensor_t;

  void analogsensor_init(analogsensor_t *sensor);
  void analogsensor_deinit(analogsensor_t *sensor);
  float analogsensor_get_value(analogsensor_t *sensor);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __ANALOGSENSOR_H__
