#include "web_server.h"
#include "hub_controller.h"
#include "mqtt_manager.h"

bool auto_discovery = false;

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/switch", HTTP_GET, handleSwitch);
  server.on("/state", HTTP_GET, handleState);
  server.on("/discovery_on", HTTP_GET, handleDiscoveryOn);
  server.on("/discovery_off", HTTP_GET, handleDiscoveryOff);
}

void handleRoot() {
  String html = "<html><head>";
  html += "<title>UGreen USB Switch</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family: Arial; text-align: center; color: #ffffff; background:#000000; padding: 5rem;} #switchButton{background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; text-decoration: none; font-size: 16px; margin: 4px 2px; cursor: pointer;} #switchButtonRed{background-color: #FF0000; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; text-decoration: none; font-size: 16px; margin: 4px 2px; cursor: pointer;}</style>";
  html += "<script>function updateState() {fetch('/state').then(response => response.text()).then(state => {document.getElementById('state').innerText = state;document.getElementById('switchButton').innerText = 'Switch to ' + (state === 'PC' ? 'Mac' : 'PC');});} setInterval(updateState, 2000);</script>";
  html += "</head><body>";
  html += "<h1>UGreen USB Switch</h1>";
  html += "<h2>Current IP: <span id='ipState'>" + STATIC_IP.toString() + "</span></h2><br/><br/><br/>";
  html += "<p>Current State: <span id='state'>" + hubState + "</span></p>";
  html += String("<a href='/switch' id='switchButton'>Switch to ") + (hubState == "PC" ? "Mac" : "PC") + "</a><br/><br/>";
  html += String("<h2>Auto Discovery: <span id='ipState'>") + (auto_discovery ? "ON" : "OFF") + "</span></h2>";
  html += "<a href='/discovery_on' id='switchButton'>Add Device To HA</a>";
  html += "<a href='/discovery_off' id='switchButtonRed'>Remove Device from HA</a>";
  html += "<h3>OTA Update</h3>";
  html += "<a href='/update' id='switchButton'>Update Firmware</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSwitch() {
  switchHub();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleState() {
  updateHubState();
  server.send(200, "text/plain", hubState);
}

void handleDiscoveryOn() {
  delay(200);
  auto_discovery = true;
  haDiscovery();
  server.send(200, "text/html", "<h1>Discovery ON...<h1><h3>Home Assistant MQTT Discovery enabled</h3><a href='/'>Back</a>");
}

void handleDiscoveryOff() {
  delay(200);
  auto_discovery = false;
  haRemoveDevice();
  server.send(200, "text/html", "<h1>Discovery OFF...<h1><h3>Home Assistant MQTT Discovery disabled. Previous entities removed.</h3><a href='/'>Back</a>");
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