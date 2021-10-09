#include <stdio.h>

#include "hal/gpio_hal.h"

#include "soilsensor.h"
#include "analogsensor.h"

#define GROVE_DIGITAL_INPUT 32

static analogsensor_t s_soilsensor = {
  .bitwidth = ADC_WIDTH_BIT_12,
  .channel = ADC1_CHANNEL_5,
  .atten = ADC_ATTEN_DB_11
};

void soilsensor_init(void)
{
  analogsensor_init(&s_soilsensor);
}

void soilsensor_deinit(void)
{
  analogsensor_deinit(&s_soilsensor);
}

int soilsensor_get_value(void)
{
  return analogsensor_get_value(&s_soilsensor);
}
