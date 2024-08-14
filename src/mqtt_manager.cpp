#include "mqtt_manager.h"
#include "wifi_manager.h"
#include "hub_controller.h"
#include <ArduinoJson.h>

void setupMQTT() {
  pubsubClient.setServer(MQTT_BROKER, MQTT_PORT);
  pubsubClient.setBufferSize(512);
  pubsubClient.setCallback(callback);
}

void reconnectMQTT() {
  while (!pubsubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "USBHubSwitch-" + String(random(0xffff), HEX);
    if (pubsubClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, MQTT_AVAILABILITY_TOPIC, 0, true, "offline")) {
      Serial.println("connected");
      pubsubClient.subscribe(MQTT_SWITCH_TOPIC);
      pubsubClient.publish(MQTT_STATE_TOPIC, hubState.c_str(), true);
      haDiscovery();
      Serial.println("Subscribed to topic and published initial state");
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubsubClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == MQTT_SWITCH_TOPIC) {
    if (message != hubState) {
      switchHub();
    } 
  }
}

void publishState() {
  pubsubClient.publish(MQTT_STATE_TOPIC, hubState.c_str(), true);
}

void publishStateIfNeeded() {
  static unsigned long lastStatePublish = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatePublish >= STATE_PUBLISH_INTERVAL) {
    publishState();
    lastStatePublish = currentMillis;
  }
}

void haDiscovery() {
  char topic[128];
  char buffer1[512];
  char uid[128];
  DynamicJsonDocument doc(1024);

  Serial.println("Discovering new devices...");

  strcpy(topic, "homeassistant/switch/");
  strcat(topic, devUniqueID);
  strcat(topic, "/config");

  strcpy(uid, devUniqueID);
  strcat(uid, "S");

  doc["name"] = "Ugreen USB Hub Switch";
  doc["uniq_id"] = uid;
  doc["stat_t"] = MQTT_STATE_TOPIC;
  doc["cmd_t"] = MQTT_SWITCH_TOPIC;
  doc["pl_on"] = "Mac";
  doc["pl_off"] = "PC";
  doc["stat_on"] = "Mac";
  doc["stat_off"] = "PC";
  doc["icon"] = "mdi:usb-port";
  doc["optimistic"] = false;
  doc["retain"] = true;

  JsonObject device = doc.createNestedObject("device");
  device["name"] = "MQTT USB Hub Switch";
  device["ids"] = "mymqttdevice01";
  device["mf"] = "DIY";
  device["mdl"] = "ESP32";

  serializeJson(doc, buffer1);   

  pubsubClient.publish(topic, buffer1, true);    

  Serial.println("All devices added!");
}

void haRemoveDevice() {
  char topic[128];
  Serial.println("Removing discovered devices...");
  strcpy(topic, "homeassistant/switch/");
  strcat(topic, devUniqueID);
  strcat(topic, "/config");
  pubsubClient.publish(topic, "");
  Serial.println("Devices Removed...");
}