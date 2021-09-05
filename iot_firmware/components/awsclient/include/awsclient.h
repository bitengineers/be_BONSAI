#pragma once

typedef struct _awsclient_mqtt_config {
} awsclient_mqtt_config_t;

void awsclient_mqtt_init(awsclient_mqtt_config_t *config);
void awsclient_mqtt_deinit(awsclient_mqtt_config_t *config);
void awsclient_shadow_init(void);
void awsclient_shadow_update(void);
