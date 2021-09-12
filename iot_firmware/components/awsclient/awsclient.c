#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "awsclient.h"

#define TAG  "AWSCLIENT"

static AWS_IoT_Client s_aws_client;
static volatile uint8_t s_updateInProgress = 0;
static void shadow_update_status_cb(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                    const char *pReceivedJsonDocument, void *pContextData);

void awsclient_shadow_init(awsclient_config_t *config)
{
  IoT_Error_t res = FAILURE;
  res = aws_iot_shadow_init(&s_aws_client, &(config->shadow_params));
  ESP_LOGI(TAG, "Shadow init: host = %s, port = %d", config->shadow_params.pHost, config->shadow_params.port);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_init failed");
    // abort();
    return;
  }

  ShadowConnectParameters_t param = config->shadow_connect_params;
  ESP_LOGI(TAG, "Shadow Connect: thingName = %s, clientId = %s", param.pMyThingName, param.pMqttClientId);
  res = aws_iot_shadow_connect(&s_aws_client, &param);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_connect failed: reason = %d", res);
    // abort();
    return;
  }
  res = aws_iot_shadow_set_autoreconnect_status(&s_aws_client, true);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_autoreconnect_status failed");
    // abort();
    return;
  }
}

void awsclient_shadow_deinit(awsclient_config_t *config)
{
  aws_iot_shadow_disconnect(&s_aws_client);
}

void awsclient_shadow_update(awsclient_config_t *config, char *jsonBuffer, size_t jsonBufferSize)
{
  IoT_Error_t res = FAILURE;
  res = aws_iot_shadow_update(&s_aws_client, config->shadow_connect_params.pMyThingName, jsonBuffer,
                              shadow_update_status_cb, NULL, config->timeout_sec, true);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_init_json_document failed");
    // abort();
    return;
  }

  s_updateInProgress = true;
}

static void shadow_update_status_cb(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                    const char *pReceivedJsonDocument, void *pContextData) {
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    s_updateInProgress = false;

    if(SHADOW_ACK_TIMEOUT == status) {
        ESP_LOGE(TAG, "Update timed out");
    } else if(SHADOW_ACK_REJECTED == status) {
        ESP_LOGE(TAG, "Update rejected");
    } else if(SHADOW_ACK_ACCEPTED == status) {
        ESP_LOGI(TAG, "Update accepted");
    }
  }
