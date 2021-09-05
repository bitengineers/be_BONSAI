idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "."
  REQUIRES wificlient analogsensor soilsensor esp32_axp192 awsclient)
