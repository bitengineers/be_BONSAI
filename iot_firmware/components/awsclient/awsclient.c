#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_error.h"
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
    awsclient_log_error(res);
    return;
  }
  ShadowConnectParameters_t param = config->shadow_connect_params;
  res = aws_iot_shadow_connect(&s_aws_client, &param);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_connect failed: reason = %d", res);
    awsclient_log_error(res);
    // abort();
    return;
  }
  res = aws_iot_shadow_set_autoreconnect_status(&s_aws_client, true);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_autoreconnect_status failed");
    awsclient_log_error(res);
    return;
  }
  awsclient_shadow_subscribe_topics(config);
}

void awsclient_shadow_connect(awsclient_config_t *config)
{
  ShadowConnectParameters_t param = config->shadow_connect_params;
  ESP_LOGI(TAG, "Shadow Connect: thingName = %s, clientId = %s", param.pMyThingName, param.pMqttClientId);
  res = aws_iot_shadow_connect(&s_aws_client, &param);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_connect failed: reason = %d", res);
    awsclient_log_error(res);
    // abort();
    return;
  }
  if (res == NETWORK_ATTEMPTING_RECONNECT || res == NETWORK_RECONNECTED  || res == SUCCESS) {
    ESP_LOGI(TAG, "aws_iot_shadow_yeild with param 200.");
    awsclient_log_error(res);
    res = aws_iot_shadow_yield(&s_aws_client, 200);
    if (res == NETWORK_ATTEMPTING_RECONNECT || awsclient_is_updating_shadow()) {
      ESP_LOGI(TAG, "aws_iot_shadow_yeild with param 1000.");
      awsclient_log_error(res);
      res = aws_iot_shadow_yield(&s_aws_client, 1000);
    }
  }
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
  aws_iot_shadow_disconnect(&s_aws_client);
  aws_iot_mqtt_free(&s_aws_client);
  s_updateInProgress = 0;
  res = FAILURE;
}

void awsclient_shadow_update(awsclient_config_t *config, char *jsonBuffer, size_t jsonBufferSize)
{
  if (!aws_iot_mqtt_is_client_connected(&s_aws_client)) {
    ESP_LOGI(TAG, "aws_iot_mqtt client was not connected. re-initialize it.");
    aws_iot_mqtt_free(&s_aws_client);
    awsclient_shadow_init(config);
  }
  res = aws_iot_shadow_update(&s_aws_client, config->shadow_connect_params.pMyThingName, jsonBuffer,
                              shadow_update_status_cb, NULL, config->timeout_sec, true);
  if (res != SUCCESS) {
    ESP_LOGE(TAG, "aws_iot_shadow_update failed: return value = %d", res);
    awsclient_log_error(res);
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


void awsclient_log_error(IoT_Error_t err)
{
  switch(err) {
	/** Returned when the Network physical layer is connected */
  case(NETWORK_PHYSICAL_LAYER_CONNECTED):
    ESP_LOGI(TAG, "NETWORK_PHYSICAL_LAYER_CONNECTED");
    break;
	/** Returned when the Network is manually disconnected */
  case(NETWORK_MANUALLY_DISCONNECTED):
    ESP_LOGI(TAG, "NETWORK_MANUALLY_DISCONNECTED");
      break;
	/** Returned when the Network is disconnected and the reconnect attempt is in progress */
  case(NETWORK_ATTEMPTING_RECONNECT):
    ESP_LOGI(TAG, "NETWORK_ATTEMPTING_RECONNECT");
    break;
	/** Return value of yield function to indicate auto-reconnect was successful */
  case(NETWORK_RECONNECTED):
    ESP_LOGI(TAG, "NETWORK_RECONNECTED");
    break;
	/** Returned when a read attempt is made on the TLS buffer and it is empty */
  case(MQTT_NOTHING_TO_READ):
    ESP_LOGI(TAG, "MQTT_NOTHING_TO_READ");
    break;
	/** Returned when a connection request is successful and packet response is connection accepted */
  case(MQTT_CONNACK_CONNECTION_ACCEPTED):
    ESP_LOGI(TAG, "MQTT_CONNACK_CONNECTION_ACCEPTED");
    break;
    /** Success return value - no error occurred */
  case(SUCCESS):
    ESP_LOGI(TAG, "SUCCESS");
    break;
    /** A generic error. Not enough information for a specific error code */
  case(FAILURE):
    ESP_LOGI(TAG, "FAILURE");
    break;
    /** A required parameter was passed as null */
  case(NULL_VALUE_ERROR):
    ESP_LOGI(TAG, "NULL_VALUE_ERROR");
    break;
    /** The TCP socket could not be established */
  case(TCP_CONNECTION_ERROR):
    ESP_LOGI(TAG, "TCP_CONNECTION_ERROR");
    break;
    /** The TLS handshake failed */
  case(SSL_CONNECTION_ERROR):
    ESP_LOGI(TAG, "SSL_CONNECTION_ERROR");
    break;
    /** Error associated with setting up the parameters of a Socket */
  case(TCP_SETUP_ERROR):
    ESP_LOGI(TAG, "TCP_SETUP_ERROR");
    break;
    /** A timeout occurred while waiting for the TLS handshake to complete. */
  case(NETWORK_SSL_CONNECT_TIMEOUT_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_CONNECT_TIMEOUT_ERROR");
    break;
    /** A Generic write error based on the platform used */
  case(NETWORK_SSL_WRITE_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_WRITE_ERROR");
    break;
    /** SSL initialization error at the TLS layer */
  case(NETWORK_SSL_INIT_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_INIT_ERROR");
    break;
    /** An error occurred when loading the certificates.  The certificates could not be located or are incorrectly formatted. */
  case(NETWORK_SSL_CERT_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_CERT_ERROR");
    break;
    /** SSL Write times out */
  case(NETWORK_SSL_WRITE_TIMEOUT_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_WRITE_TIMEOUT_ERROR");
    break;
    /** SSL Read times out */
  case(NETWORK_SSL_READ_TIMEOUT_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_READ_TIMEOUT_ERROR");
    break;
    /** A Generic error based on the platform used */
  case(NETWORK_SSL_READ_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_READ_ERROR");
    break;
    /** Returned when the Network is disconnected and reconnect is either disabled or physical layer is disconnected */
  case(NETWORK_DISCONNECTED_ERROR):
    ESP_LOGI(TAG, "NETWORK_DISCONNECTED_ERROR");
    break;
    /** Returned when the Network is disconnected and the reconnect attempt has timed out */
  case(NETWORK_RECONNECT_TIMED_OUT_ERROR):
    ESP_LOGI(TAG, "NETWORK_RECONNECT_TIMED_OUT_ERROR");
    break;
    /** Returned when the Network is already connected and a connection attempt is made */
  case(NETWORK_ALREADY_CONNECTED_ERROR):
    ESP_LOGI(TAG, "NETWORK_ALREADY_CONNECTED_ERROR");
    break;
	/** Network layer Error Codes */
	/** Network layer Random number generator seeding failed */
  case(NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED):
    ESP_LOGI(TAG, "NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED");
    break;
	/** A generic error code for Network layer errors */
  case(NETWORK_SSL_UNKNOWN_ERROR):
    ESP_LOGI(TAG, "NETWORK_SSL_UNKNOWN_ERROR");
    break;
	/** Returned when the physical layer is disconnected */
  case(NETWORK_PHYSICAL_LAYER_DISCONNECTED):
    ESP_LOGI(TAG, "NETWORK_PHYSICAL_LAYER_DISCONNECTED");
    break;
	/** Returned when the root certificate is invalid */
  case(NETWORK_X509_ROOT_CRT_PARSE_ERROR):
    ESP_LOGI(TAG, "NETWORK_X509_ROOT_CRT_PARSE_ERROR");
    break;
	/** Returned when the device certificate is invalid */
  case(NETWORK_X509_DEVICE_CRT_PARSE_ERROR):
    ESP_LOGI(TAG, "NETWORK_X509_DEVICE_CRT_PARSE_ERROR");
    break;
	/** Returned when the private key failed to parse */
  case(NETWORK_PK_PRIVATE_KEY_PARSE_ERROR):
    ESP_LOGI(TAG, "NETWORK_PK_PRIVATE_KEY_PARSE_ERROR");
    break;
	/** Returned when the network layer failed to open a socket */
  case(NETWORK_ERR_NET_SOCKET_FAILED):
    ESP_LOGI(TAG, "NETWORK_ERR_NET_SOCKET_FAILED");
    break;
	/** Returned when the server is unknown */
  case(NETWORK_ERR_NET_UNKNOWN_HOST):
    ESP_LOGI(TAG, "NETWORK_ERR_NET_UNKNOWN_HOST");
    break;
	/** Returned when connect request failed */
  case(NETWORK_ERR_NET_CONNECT_FAILED):
    ESP_LOGI(TAG, "NETWORK_ERR_NET_CONNECT_FAILED");
    break;
	/** Returned when there is nothing to read in the TLS read buffer */
  case(NETWORK_SSL_NOTHING_TO_READ):
    ESP_LOGI(TAG, "NETWORK_SSL_NOTHING_TO_READ");
    break;
	/** A connection could not be established. */
  case(MQTT_CONNECTION_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNECTION_ERROR");
    break;
	/** A timeout occurred while waiting for the TLS handshake to complete */
  case(MQTT_CONNECT_TIMEOUT_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNECT_TIMEOUT_ERROR");
    break;
	/** A timeout occurred while waiting for the TLS request complete */
  case(MQTT_REQUEST_TIMEOUT_ERROR):
    ESP_LOGI(TAG, "MQTT_REQUEST_TIMEOUT_ERROR");
    break;
	/** The current client state does not match the expected value */
  case(MQTT_UNEXPECTED_CLIENT_STATE_ERROR):
    ESP_LOGI(TAG, "MQTT_UNEXPECTED_CLIENT_STATE_ERROR");
    break;
	/** The client state is not idle when request is being made */
  case(MQTT_CLIENT_NOT_IDLE_ERROR):
    ESP_LOGI(TAG, "MQTT_CLIENT_NOT_IDLE_ERROR");
    break;
	/** The MQTT RX buffer received corrupt or unexpected message  */
  case(MQTT_RX_MESSAGE_PACKET_TYPE_INVALID_ERROR):
    ESP_LOGI(TAG, "MQTT_RX_MESSAGE_PACKET_TYPE_INVALID_ERROR");
    break;
	/** The MQTT RX buffer received a bigger message. The message will be dropped  */
  case(MQTT_RX_BUFFER_TOO_SHORT_ERROR):
    ESP_LOGI(TAG, "MQTT_RX_BUFFER_TOO_SHORT_ERROR");
    break;
	/** The MQTT TX buffer is too short for the outgoing message. Request will fail  */
  case(MQTT_TX_BUFFER_TOO_SHORT_ERROR):
    ESP_LOGI(TAG, "MQTT_TX_BUFFER_TOO_SHORT_ERROR");
    break;
	/** The client is subscribed to the maximum possible number of subscriptions  */
  case(MQTT_MAX_SUBSCRIPTIONS_REACHED_ERROR):
    ESP_LOGI(TAG, "MQTT_MAX_SUBSCRIPTIONS_REACHED_ERROR");
    break;
	/** Failed to decode the remaining packet length on incoming packet */
  case(MQTT_DECODE_REMAINING_LENGTH_ERROR):
    ESP_LOGI(TAG, "MQTT_DECODE_REMAINING_LENGTH_ERROR");
    break;
	/** Connect request failed with the server returning an unknown error */
  case(MQTT_CONNACK_UNKNOWN_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_UNKNOWN_ERROR");
    break;
	/** Connect request failed with the server returning an unacceptable protocol version error */
  case(MQTT_CONNACK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR");
    break;
	/** Connect request failed with the server returning an identifier rejected error */
  case(MQTT_CONNACK_IDENTIFIER_REJECTED_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_IDENTIFIER_REJECTED_ERROR");
    break;
	/** Connect request failed with the server returning an unavailable error */
  case(MQTT_CONNACK_SERVER_UNAVAILABLE_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_SERVER_UNAVAILABLE_ERROR");
    break;
	/** Connect request failed with the server returning a bad userdata error */
  case(MQTT_CONNACK_BAD_USERDATA_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_BAD_USERDATA_ERROR");
    break;
	/** Connect request failed with the server failing to authenticate the request */
  case(MQTT_CONNACK_NOT_AUTHORIZED_ERROR):
    ESP_LOGI(TAG, "MQTT_CONNACK_NOT_AUTHORIZED_ERROR");
    break;
	/** An error occurred while parsing the JSON string.  Usually malformed JSON. */
  case(JSON_PARSE_ERROR):
    ESP_LOGI(TAG, "JSON_PARSE_ERROR");
    break;
	/** Shadow: The response Ack table is currently full waiting for previously published updates */
  case(SHADOW_WAIT_FOR_PUBLISH):
    ESP_LOGI(TAG, "SHADOW_WAIT_FOR_PUBLISH");
    break;
	/** Any time an snprintf writes more than size value, this error will be returned */
  case(SHADOW_JSON_BUFFER_TRUNCATED):
    ESP_LOGI(TAG, "SHADOW_JSON_BUFFER_TRUNCATED");
    break;
	/** Any time an snprintf encounters an encoding error or not enough space in the given buffer */
  case(SHADOW_JSON_ERROR):
    ESP_LOGI(TAG, "SHADOW_JSON_ERROR");
    break;
	/** Mutex initialization failed */
  case(MUTEX_INIT_ERROR):
    ESP_LOGI(TAG, "MUTEX_INIT_ERROR");
    break;
	/** Mutex lock request failed */
  case(MUTEX_LOCK_ERROR):
    ESP_LOGI(TAG, "MUTEX_LOCK_ERROR");
    break;
	/** Mutex unlock request failed */
  case(MUTEX_UNLOCK_ERROR):
    ESP_LOGI(TAG, "MUTEX_UNLOCK_ERROR");
    break;
	/** Mutex destroy failed */
  case(MUTEX_DESTROY_ERROR):
    ESP_LOGI(TAG, "MUTEX_DESTROY_ERROR");
    break;
	/** Input argument exceeded the allowed maximum size */
  case(MAX_SIZE_ERROR):
    ESP_LOGI(TAG, "MAX_SIZE_ERROR");
    break;
	/** Some limit has been exceeded, e.g. the maximum number of subscriptions has been reached */
  case(LIMIT_EXCEEDED_ERROR):
    ESP_LOGI(TAG, "LIMIT_EXCEEDED_ERROR");
    break;
	/** Invalid input topic type */
  case(INVALID_TOPIC_TYPE_ERROR):
    ESP_LOGI(TAG, "INVALID_TOPIC_TYPE_ERROR");
    break;
  }
}
