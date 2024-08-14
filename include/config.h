#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi configuration
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* HOSTNAME;

// Static IP configuration
extern const IPAddress STATIC_IP;
extern const IPAddress GATEWAY;
extern const IPAddress SUBNET;

// MQTT configuration
extern const char* MQTT_BROKER;
extern const int MQTT_PORT;
extern const char* MQTT_USERNAME;
extern const char* MQTT_PASSWORD;

// MQTT topics
extern const char* MQTT_DISCOVERY_PREFIX;
extern const char* MQTT_DEVICE_NAME;
extern const char* MQTT_SWITCH_TOPIC;
extern const char* MQTT_STATE_TOPIC;
extern const char* MQTT_AVAILABILITY_TOPIC;

// Pin configuration
const int LED_PIN = 15;
const int SWITCH_PIN = 35;
const int MONITOR_PIN = 39;

// Timing constants
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long STATE_CHANGE_THRESHOLD = 1000;
const unsigned long STATE_PUBLISH_INTERVAL = 60000;

// Other constants
const int MAX_WIFI_CONNECT_ATTEMPTS = 20;
const int WIFI_RETRY_DELAY = 2000;

#endif // CONFIG_H