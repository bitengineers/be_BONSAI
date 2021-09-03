#include <stdio.h>
#include "analogsensor.h"

void analogsensor_init(analogsensor_t *sensor)
{
  adc1_config_width(sensor->bitwidth);
  adc1_config_channel_atten(sensor->channel, sensor->atten);
}

void analogsensor_deinit(analogsensor_t *sensor)
{
}

float analogsensor_get_value(analogsensor_t *sensor)
{
  return adc1_get_raw(sensor->channel);
}

