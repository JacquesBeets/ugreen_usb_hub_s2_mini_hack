#include "config.h"
#include "env.h"

// Define the configuration variables
const char* WIFI_SSID = ENV_WIFI_SSID;
const char* WIFI_PASSWORD = ENV_WIFI_PASSWORD;
const char* HOSTNAME = "UGreen_USB_Switch";

const IPAddress STATIC_IP = IPAddress(ENV_STATIC_IP);
const IPAddress GATEWAY = IPAddress(ENV_GATEWAY);
const IPAddress SUBNET = IPAddress(ENV_SUBNET);

const char* MQTT_BROKER = ENV_MQTT_BROKER;
const int MQTT_PORT = ENV_MQTT_PORT;
const char* MQTT_USERNAME = ENV_MQTT_USERNAME;
const char* MQTT_PASSWORD = ENV_MQTT_PASSWORD;

const char* MQTT_DISCOVERY_PREFIX = "homeassistant";
const char* MQTT_DEVICE_NAME = "ugreen_usb_hub_switch";
const char* MQTT_SWITCH_TOPIC = "home/usb_hub_switch/switch/set";
const char* MQTT_STATE_TOPIC = "home/usb_hub_switch/state";
const char* MQTT_AVAILABILITY_TOPIC = "stat/usb_hub_switch/availability";
