#include "web_server.h"
#include "hub_controller.h"
#include "mqtt_manager.h"
#include "SPIFFS.h"

bool auto_discovery = false;

void setupWebServer() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/switch", HTTP_POST, handleSwitch);
    server.on("/state", HTTP_GET, handleState);
    server.on("/ip", HTTP_GET, handleIP);
    server.on("/discovery_state", HTTP_GET, handleDiscoveryState);
    server.on("/discovery_on", HTTP_POST, handleDiscoveryOn);
    server.on("/discovery_off", HTTP_POST, handleDiscoveryOff);

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "404: Not Found");
        }
    });
}

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    String contentType = getContentType(path);
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

void handleRoot() {
    handleFileRead("/index.html");
}

void handleSwitch() {
    switchHub();
    server.send(200, "text/plain", "OK");
}

void handleState() {
    updateHubState();
    server.send(200, "text/plain", hubState);
}

void handleIP() {
    server.send(200, "text/plain", STATIC_IP.toString());
}

void handleDiscoveryState() {
    server.send(200, "text/plain", auto_discovery ? "ON" : "OFF");
}

void handleDiscoveryOn() {
    auto_discovery = true;
    haDiscovery();
    server.send(200, "text/plain", "OK");
}

void handleDiscoveryOff() {
    auto_discovery = false;
    haRemoveDevice();
    server.send(200, "text/plain", "OK");
}

void elegantOTACallbackInit() {
    ElegantOTA.onStart([]() {
        Serial.println("OTA update started!");
    });

    ElegantOTA.onProgress([](size_t current, size_t final) {
        static unsigned long ota_progress_millis = 0;
        if (millis() - ota_progress_millis > 1000) {
            ota_progress_millis = millis();
            Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
        }
    });

    ElegantOTA.onEnd([](bool success) {
        if (success) {
            Serial.println("OTA update finished successfully!");
        } else {
            Serial.println("There was an error during OTA update!");
        }
    });
}