#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"

void setupMQTT();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void publishState();
void publishStateIfNeeded();
void haDiscovery();
void haRemoveDevice();

extern PubSubClient pubsubClient;

#endif // MQTT_MANAGER_H