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
static IoT_Error_t res = FAILURE;
static volatile uint8_t s_updateInProgress = 0;

static char s_topic_delete_accepted[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_delete_rejected[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_get_accepted[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_get_rejected[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_update_accepted[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_update_rejected[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_update_delta[MAX_SHADOW_TOPIC_LENGTH_BYTES];
static char s_topic_update_documents[MAX_SHADOW_TOPIC_LENGTH_BYTES];

static void awsclient_shadow_subscribe_topics(awsclient_config_t *config);
static void _awsclient_shadow_subscribe_topic(awsclient_config_t *config, char *topic_str, const char *topic_template);
static void awsclient_shadow_callback(AWS_IoT_Client *pClient, char *pTopicName, uint16_t topicNameLen,
									  IoT_Publish_Message_Params *pParams, void *pClientData);
static uint8_t awsclient_is_updating_shadow(void);
static void shadow_update_status_cb(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                    const char *pReceivedJsonDocument, void *pContextData);


void awsclient_shadow_init(awsclient_config_t *config)
{
  res = aws_iot_shadow_init(&s_aws_client, &(config->shadow_params));
  ESP_LOGI(TAG, "Shadow init: host = %s, port = %d", config->shadow_params.pHost, config->shadow_params.port);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_init failed");
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
  awsclient_shadow_subscribe_topics(config);
}

static void awsclient_shadow_subscribe_topics(awsclient_config_t *config)
{
  _awsclient_shadow_subscribe_topic(config, s_topic_delete_accepted, "$aws/things/%s/shadow/delete/accepted");
  _awsclient_shadow_subscribe_topic(config, s_topic_delete_rejected, "$aws/things/%s/shadow/delete/rejected");
  _awsclient_shadow_subscribe_topic(config, s_topic_get_accepted, "$aws/things/%s/shadow/get/accepted");
  _awsclient_shadow_subscribe_topic(config, s_topic_get_rejected, "$aws/things/%s/shadow/get/rejected");
  _awsclient_shadow_subscribe_topic(config, s_topic_update_accepted, "$aws/things/%s/shadow/update/accepted");
  _awsclient_shadow_subscribe_topic(config, s_topic_update_rejected, "$aws/things/%s/shadow/update/rejected");
  _awsclient_shadow_subscribe_topic(config, s_topic_update_delta, "$aws/things/%s/shadow/update/delta");
  _awsclient_shadow_subscribe_topic(config, s_topic_update_documents, "$aws/things/%s/shadow/update/documents");
}

static void _awsclient_shadow_subscribe_topic(awsclient_config_t *config, char *topic_str, const char *topic_template)
{
  uint16_t len;
  snprintf(topic_str, MAX_SHADOW_TOPIC_LENGTH_BYTES,
           topic_template, config->shadow_connect_params.pMyThingName);
  len = (uint16_t) strlen(topic_str);
  res = aws_iot_mqtt_subscribe(&s_aws_client, topic_str, len, QOS1,
                               awsclient_shadow_callback, (void*)config);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_subscribe %s failed", topic_str);
  }
}

void awsclient_shadow_deinit(awsclient_config_t *config)
{
  res = aws_iot_shadow_disconnect(&s_aws_client);
}

void awsclient_shadow_update(awsclient_config_t *config, char *jsonBuffer, size_t jsonBufferSize)
{
  IoT_Error_t res = FAILURE;
  if (res == NETWORK_ATTEMPTING_RECONNECT || res == NETWORK_RECONNECTED  || res == SUCCESS) {
    res = aws_iot_shadow_yield(&s_aws_client, 200);
    if (res == NETWORK_ATTEMPTING_RECONNECT || awsclient_is_updating_shadow()) {
      ESP_LOGI(TAG, "aws_iot_shadow_yeild with param 1000");
      res = aws_iot_shadow_yield(&s_aws_client, 1000);
    }
  }
  res = aws_iot_shadow_update(&s_aws_client, config->shadow_connect_params.pMyThingName, jsonBuffer,
                              shadow_update_status_cb, NULL, config->timeout_sec, true);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_update failed: return value = %d", res);
    // abort();
    return;
  }

  s_updateInProgress = true;
}

void awsclient_shadow_callback(AWS_IoT_Client *pClient, char *pTopicName, uint16_t topicNameLen,
									  IoT_Publish_Message_Params *pParams, void *pClientData)
{
  ESP_LOGI(TAG, "[%s] callback: ", pTopicName);
}

static uint8_t awsclient_is_updating_shadow(void)
{
  return s_updateInProgress;
}


IoT_Error_t awsclient_err(void) {
  return res;
}


void shadow_update_status_cb(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
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
