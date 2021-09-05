#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  void soilsensor_init(void);
  void soilsensor_deinit(void);
  int soilsensor_get_value(void);

#ifdef __cplusplus
}
#endif // __cplusplus
