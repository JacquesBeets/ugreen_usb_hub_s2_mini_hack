#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

void setupWiFi();
void reconnectWiFi();
void printWifiStatus();
void createDiscoveryUniqueID();

extern byte macAddr[6];
extern char uidPrefix[];
extern char devUniqueID[30];

#endif // WIFI_MANAGER_H