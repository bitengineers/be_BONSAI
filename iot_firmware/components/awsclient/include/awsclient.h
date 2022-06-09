#pragma once

#include "aws_iot_config.h"
#include "aws_iot_error.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

typedef struct _awsclient_config {
  ShadowInitParameters_t shadow_params;
  ShadowConnectParameters_t shadow_connect_params;
  uint32_t timeout_sec;
} awsclient_config_t;

void awsclient_shadow_init(awsclient_config_t *config);

void awsclient_shadow_deinit(awsclient_config_t *config);

void awsclient_shadow_connect(awsclient_config_t *config);

void awsclient_shadow_disconnect(awsclient_config_t *config);

void awsclient_shadow_update(awsclient_config_t *config, char *jsonBuffer, size_t jsonBufferSize);

void awsclient_shadow_yield(awsclient_config_t *config);

IoT_Error_t awsclient_err(void);

void awsclient_log_error(IoT_Error_t err);
