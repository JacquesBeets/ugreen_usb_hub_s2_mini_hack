#include "wifi_manager.h"

byte macAddr[6];
char uidPrefix[] = "rctdev";
char devUniqueID[30];

void setupWiFi() {
  WiFi.setHostname(HOSTNAME);

  if (!WiFi.config(STATIC_IP, GATEWAY, SUBNET)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int wifiConnectAttempts = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
    wifiConnectAttempts++;

    if (wifiConnectAttempts >= MAX_WIFI_CONNECT_ATTEMPTS) {
      Serial.println("\nFailed to connect to WiFi. Retrying...");
      delay(10000);
      wifiConnectAttempts = 0;
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  WiFi.macAddress(macAddr);
  createDiscoveryUniqueID();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectWiFi() {
  Serial.println("WiFi connection lost. Reconnecting...");
  WiFi.reconnect();
}

void printWifiStatus() {
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 30000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(STATIC_IP.toString());
    } else if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("WiFi shield not present");
    } else {
      Serial.println("WiFi disconnected");
    }
    lastPrintTime = millis();
  }
}

void createDiscoveryUniqueID() {
  strcpy(devUniqueID, uidPrefix);
  int preSizeBytes = sizeof(uidPrefix);
  int j = 0;
  for (int i = 2; i >= 0; i--) {
    sprintf(&devUniqueID[(preSizeBytes - 1) + (j)], "%02X", macAddr[i]);
    j = j + 2;
  }
  Serial.print("Unique ID: ");
  Serial.println(devUniqueID);
}