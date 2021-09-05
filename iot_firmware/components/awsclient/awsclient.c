#include <stdio.h>

#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "awsclient.h"

#define TAG  "AWSCLIENT"

static AWS_IoT_Client s_aws_mqtt_client;

void awsclient_mqtt_init(awsclient_mqtt_config_t *config)
{
}

void awsclient_mqtt_deinit(awsclient_mqtt_config_t *config)
{
}

void awsclient_shadow_init(void)
{
}

void awsclient_shadow_update(void)
{
}
