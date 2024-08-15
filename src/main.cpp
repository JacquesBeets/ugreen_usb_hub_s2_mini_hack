#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "web_server.h"
#include "hub_controller.h"

WiFiClient espClient;
PubSubClient pubsubClient(espClient);
WebServer server(80);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SWITCH_PIN, OUTPUT);
  digitalWrite(SWITCH_PIN, HIGH);
  pinMode(MONITOR_PIN, INPUT_PULLUP);

  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return;
  }

  setupWiFi();
  setupMQTT();
  setupWebServer();
  setupHubController();

  ElegantOTA.begin(&server);
  elegantOTACallbackInit();
  server.begin();
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();
  
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
  
  if (!pubsubClient.connected()) {
    reconnectMQTT();
  }
  
  pubsubClient.loop();
  updateHubState();
  publishStateIfNeeded();
  printWifiStatus();
}