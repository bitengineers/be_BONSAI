idf_component_register(
  SRCS "main.c" "app_sleep.c" "app_sensors.c"
  INCLUDE_DIRS "."
  REQUIRES
      nvs_flash
      wificlient
      analogsensor
      soilsensor
      esp32_axp192
      esp_pahub
      esp_pbhub
      sht30
      awsclient
      esp-aws-iot)

target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/certificate.pem.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/private.pem.key" TEXT)
