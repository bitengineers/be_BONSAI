#include <stdio.h>
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
  gpio_config_t conf;
  conf.mode = GPIO_MODE_OUTPUT;
  conf.pin_bit_mask = 1ULL  << GROVE_DIGITAL_INPUT;
  conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  conf.pull_up_en = GPIO_PULLUP_DISABLE;
  conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&conf);
  // gpio_set_level((gpio_num_t)GROVE_DIGITAL_INPUT, 0);
  gpio_set_level((gpio_num_t)GROVE_DIGITAL_INPUT, 1);
  analogsensor_init(&s_soilsensor);
}

void soilsensor_deinit(void)
{
  gpio_set_level((gpio_num_t)GROVE_DIGITAL_INPUT, 0);
  analogsensor_deinit(&s_soilsensor);
}

int soilsensor_get_value(void)
{
  return analogsensor_get_value(&s_soilsensor);
}
