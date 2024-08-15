#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "config.h"

void setupWebServer();
void handleRoot();
void handleSwitch();
void handleState();
void handleIP();
void handleDiscoveryState();
void handleDiscoveryOn();
void handleDiscoveryOff();
void elegantOTACallbackInit();
bool handleFileRead(String path);  // Changed return type to bool
String getContentType(String filename);

extern WebServer server;
extern bool auto_discovery;

#endif // WEB_SERVER_H