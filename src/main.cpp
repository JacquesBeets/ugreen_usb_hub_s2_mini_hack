#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// =============================================================================
// Configuration
// =============================================================================

// Config file path
#define CONFIG_FILE "/config.json"

// Default values (used if no config exists)
char mqtt_broker[64] = "192.168.0.112";
char mqtt_port[6] = "1883";
char mqtt_username[32] = "";
char mqtt_password[32] = "";

// Fixed STA network config (no DHCP reservation available)
const IPAddress STATIC_IP(192, 168, 0, 92);
const IPAddress STATIC_GATEWAY(192, 168, 0, 1);
const IPAddress STATIC_SUBNET(255, 255, 255, 0);
const IPAddress STATIC_DNS(192, 168, 0, 1);

// Flag for saving config
bool shouldSaveConfig = false;

// WiFiManager custom parameters
WiFiManagerParameter* custom_mqtt_broker;
WiFiManagerParameter* custom_mqtt_port;
WiFiManagerParameter* custom_mqtt_username;
WiFiManagerParameter* custom_mqtt_password;

// WiFiManager instance (global for access in loop)
WiFiManager wifiManager;

// =============================================================================
// Global Objects
// =============================================================================

WebServer server(80);
WiFiClient espClient;
PubSubClient pubsubClient(espClient);

// HUB STATE
String hubState = "PC";
const int switchPin = 35;  // GPIO35 on Wemos S2 Mini
const int monitorPin = 39; // GPIO39 on Wemos S2 Mini for monitoring channel state
const int ledPin = 15;     // Onboard LED

// Auto-discover enable/disable option
bool auto_discovery = false;

// Debounce settings
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long STATE_CHANGE_THRESHOLD = 1000;
unsigned long lastDebounceTime = 0;
unsigned long lastStateChangeTime = 0;
int lastSteadyState = LOW;
int lastFlickerableState = LOW;
int currentState;

// MQTT reconnect settings (non-blocking, exponential backoff 5 s -> 60 s)
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_BACKOFF_MIN = 5000;
const unsigned long MQTT_BACKOFF_MAX = 60000;
unsigned long mqttBackoff = MQTT_BACKOFF_MIN;

// State publish interval
unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 60000;

// WiFi status print interval
unsigned long lastPrintTime = 0;

// MQTT topics and device info
byte macAddr[6];
char uidPrefix[] = "rctdev";
char devUniqueID[30];
const char* mqtt_discovery_prefix = "homeassistant";
const char* mqtt_device_name = "ugreen_usb_hub_switch";
const char* mqtt_switch_topic = "home/usb_hub_switch/switch/set";
const char* mqtt_state_topic = "home/usb_hub_switch/state";
const char* mqtt_availability_topic = "stat/usb_hub_switch/availability";

// =============================================================================
// Function Declarations
// =============================================================================

void publishState();
void haDiscovery();
void haRemoveDevice();
void loadConfig();
void saveConfig();
void setupWiFiManager();
bool mqttReconnect();

// =============================================================================
// Configuration Management
// =============================================================================

void loadConfig() {
  Serial.println("Loading config...");

  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  if (LittleFS.exists(CONFIG_FILE)) {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, buf.get());

      if (!error) {
        Serial.println("Parsed config:");

        if (doc["mqtt_broker"]) {
          strlcpy(mqtt_broker, doc["mqtt_broker"], sizeof(mqtt_broker));
          Serial.print("  MQTT Broker: ");
          Serial.println(mqtt_broker);
        }
        if (doc["mqtt_port"]) {
          strlcpy(mqtt_port, doc["mqtt_port"], sizeof(mqtt_port));
          Serial.print("  MQTT Port: ");
          Serial.println(mqtt_port);
        }
        if (doc["mqtt_username"]) {
          strlcpy(mqtt_username, doc["mqtt_username"], sizeof(mqtt_username));
          Serial.print("  MQTT Username: ");
          Serial.println(mqtt_username);
        }
        if (doc["mqtt_password"]) {
          strlcpy(mqtt_password, doc["mqtt_password"], sizeof(mqtt_password));
          Serial.println("  MQTT Password: [hidden]");
        }
      } else {
        Serial.println("Failed to parse config file");
      }
      configFile.close();
    }
  } else {
    Serial.println("No config file found, using defaults");
  }
}

void saveConfig() {
  Serial.println("Saving config...");

  JsonDocument doc;
  doc["mqtt_broker"] = mqtt_broker;
  doc["mqtt_port"] = mqtt_port;
  doc["mqtt_username"] = mqtt_username;
  doc["mqtt_password"] = mqtt_password;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();
  Serial.println("Config saved successfully");
}

// Callback for WiFiManager when config needs saving
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// =============================================================================
// Hub State Management
// =============================================================================

void updateHubState() {
  currentState = digitalRead(monitorPin);

  if (currentState != lastFlickerableState) {
    lastDebounceTime = millis();
    lastFlickerableState = currentState;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentState != lastSteadyState) {
      lastSteadyState = currentState;

      if ((millis() - lastStateChangeTime) > STATE_CHANGE_THRESHOLD) {
        String newState = (currentState == LOW) ? "Mac" : "PC";
        if (hubState != newState) {
          hubState = newState;
          lastStateChangeTime = millis();
          Serial.println("Hub state changed to: " + hubState);
          publishState();
        }
      }
    }
  }
}

void switchHub() {
  Serial.println("Switching Hub...");
  Serial.println(hubState);
  digitalWrite(switchPin, LOW);
  delay(100);
  digitalWrite(switchPin, HIGH);
  delay(500);
  updateHubState();
  Serial.println("After Switching Hub...");
  Serial.println(hubState);
}

// =============================================================================
// LED Functions
// =============================================================================

void ledOn() {
  digitalWrite(ledPin, HIGH);
}

void ledOff() {
  digitalWrite(ledPin, LOW);
}

void blinkLED(int delayTime = 1000) {
  ledOn();
  delay(delayTime);
  ledOff();
  delay(delayTime);
}

// =============================================================================
// Web Server Handlers
// =============================================================================

void handleRoot() {
  String html = "<html><head>";
  html += "<title>UGreen USB Switch</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family: Arial; text-align: center; color: #ffffff; background:#000000; padding: 2rem;}";
  html += ".btn{background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 4px;}";
  html += ".btn-red{background-color: #FF0000;}";
  html += ".btn-blue{background-color: #2196F3;}";
  html += ".info{background: #333; padding: 1rem; border-radius: 8px; margin: 1rem 0;}";
  html += "</style>";
  html += "<script>function updateState() {fetch('/state').then(response => response.text()).then(state => {document.getElementById('state').innerText = state;document.getElementById('switchButton').innerText = 'Switch to ' + (state === 'PC' ? 'Mac' : 'PC');});} setInterval(updateState, 2000);</script>";
  html += "</head><body>";
  html += "<h1>UGreen USB Switch</h1>";

  html += "<div class='info'>";
  html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>Hostname:</strong> ugreen-usb-hub</p>";
  html += "<p><strong>MQTT Broker:</strong> " + String(mqtt_broker) + ":" + String(mqtt_port) + "</p>";
  html += "<p><strong>MQTT Status:</strong> " + String(pubsubClient.connected() ? "Connected" : "Disconnected") + "</p>";
  html += "</div>";

  html += "<h2>Current State: <span id='state'>" + hubState + "</span></h2>";
  html += String("<a href='/switch' class='btn' id='switchButton'>Switch to ") + (hubState == "PC" ? "Mac" : "PC") + "</a><br/><br/>";

  html += "<h3>Home Assistant Discovery</h3>";
  html += "<p>Status: " + String(auto_discovery ? "ON" : "OFF") + "</p>";
  html += "<a href='/discovery_on' class='btn'>Add to HA</a> ";
  html += "<a href='/discovery_off' class='btn btn-red'>Remove from HA</a><br/><br/>";

  html += "<h3>Device Management</h3>";
  html += "<a href='/update' class='btn btn-blue'>OTA Update</a> ";
  html += "<a href='/reset' class='btn btn-red'>Reset WiFi Config</a>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// GET /switch            -> toggle, 303 redirect to UI (web UI button relies on this)
// GET /switch?to=Mac|PC   -> switch only if not already on target, 200 text/plain state
void handleSwitch() {
  if (!server.hasArg("to")) {
    switchHub();
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  String to = server.arg("to");
  String target;
  if (to.equalsIgnoreCase("Mac")) {
    target = "Mac";
  } else if (to.equalsIgnoreCase("PC")) {
    target = "PC";
  } else {
    server.send(400, "text/plain", "Invalid 'to' value, expected Mac or PC");
    return;
  }

  updateHubState();
  if (target != hubState) {  // same rule as the MQTT set handler
    switchHub();
    // Let the debounce in updateHubState() settle so the response reflects the new state
    unsigned long start = millis();
    while (hubState != target && millis() - start < 300) {
      delay(10);
      updateHubState();
    }
  }
  server.send(200, "text/plain", hubState);
}

// --- Diagnostics: boot counter (survives SW reset) + ring of recent WiFi events ---
RTC_NOINIT_ATTR uint32_t bootMagic;
RTC_NOINIT_ATTR uint32_t bootCount;
const uint32_t BOOT_MAGIC = 0xB007C0DE;
struct WifiEvt { uint32_t ms; uint16_t id; uint16_t reason; };
WifiEvt wifiEvents[8];
uint8_t wifiEvtHead = 0, wifiEvtCount = 0;
portMUX_TYPE wifiEvtMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t lastDisconnectMs = 0;   // millis() of last STA_DISCONNECTED (same value as ring entry)
volatile uint32_t disconnectCount = 0;
unsigned long lastManualReconnectMs = 0;
const unsigned long WIFI_RECONNECT_GRACE = 30000;

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  uint16_t reason = (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) ? info.wifi_sta_disconnected.reason : 0;
  portENTER_CRITICAL(&wifiEvtMux);
  wifiEvents[wifiEvtHead] = {millis(), (uint16_t)event, reason};
  wifiEvtHead = (wifiEvtHead + 1) % 8;
  if (wifiEvtCount < 8) wifiEvtCount++;
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastDisconnectMs = wifiEvents[(wifiEvtHead + 7) % 8].ms;
    disconnectCount++;
  }
  portEXIT_CRITICAL(&wifiEvtMux);
}

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";   case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";             case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";   case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";           case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT"; case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

void handleDiag() {
  String out;
  out += "uptime_ms=" + String(millis()) + "\n";
  out += "boot_count=" + String(bootCount) + "\n";
  out += "reset_reason=" + String(resetReasonStr(esp_reset_reason())) + "\n";
  out += "wifi_status=" + String((int)WiFi.status()) + "\n";
  out += "ip=" + WiFi.localIP().toString() + "\n";
  out += "rssi=" + String(WiFi.RSSI()) + "\n";
  out += "free_heap=" + String(ESP.getFreeHeap()) + "\n";
  out += "min_free_heap=" + String(ESP.getMinFreeHeap()) + "\n";
  out += "monitor_pin_raw=" + String(digitalRead(monitorPin)) + "\n";
  out += "hub_state=" + hubState + "\n";
  out += "switch_pin_raw=" + String(digitalRead(switchPin)) + "\n";
  out += "mqtt_backoff_ms=" + String(mqttBackoff) + "\n";
  out += "mqtt_connected=" + String(pubsubClient.connected() ? 1 : 0) + "\n";
  out += "wifi_sleep=" + String(WiFi.getSleep() ? 1 : 0) + "\n";
  out += "disconnects=" + String(disconnectCount) + "\n";
  out += "last_manual_reconnect_ms=" + String(lastManualReconnectMs) + "\n";
  WifiEvt snap[8]; uint8_t head, count;
  portENTER_CRITICAL(&wifiEvtMux);
  memcpy(snap, wifiEvents, sizeof(snap)); head = wifiEvtHead; count = wifiEvtCount;
  portEXIT_CRITICAL(&wifiEvtMux);
  out += "wifi_events=";
  for (uint8_t i = 0; i < count; i++) {
    const WifiEvt& e = snap[(head + 8 - count + i) % 8];
    if (i) out += ",";
    out += String(e.ms) + ":" + String(e.id) + ":" + String(e.reason);
  }
  out += "\n";
  server.send(200, "text/plain", out);
}

void handleState() {
  updateHubState();
  server.send(200, "text/plain", hubState);
}

void handleDiscoveryOn() {
  delay(200);
  auto_discovery = true;
  haDiscovery();
  server.send(200, "text/html", "<h1>Discovery ON</h1><h3>Home Assistant MQTT Discovery enabled</h3><a href='/'>Back</a>");
}

void handleDiscoveryOff() {
  delay(200);
  auto_discovery = false;
  haRemoveDevice();
  server.send(200, "text/html", "<h1>Discovery OFF</h1><h3>Device removed from Home Assistant</h3><a href='/'>Back</a>");
}

void handleReset() {
  server.send(200, "text/html", "<h1>Resetting WiFi Configuration...</h1><p>Device will restart in AP mode. Connect to 'UGreen-USB-Hub-Setup' network to reconfigure.</p>");
  delay(1000);
  wifiManager.resetSettings();
  LittleFS.remove(CONFIG_FILE);
  ESP.restart();
}

// =============================================================================
// OTA Callbacks
// =============================================================================

void onOTAStart() {
  Serial.println("OTA update started!");
  blinkLED();
}

unsigned long ota_progress_millis = 0;
void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress: %u / %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");
    blinkLED();
  } else {
    Serial.println("OTA update failed!");
    blinkLED(500);
  }
}

// =============================================================================
// MQTT Functions
// =============================================================================

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

void haDiscovery() {
  if (!pubsubClient.connected()) {
    Serial.println("Cannot publish discovery - MQTT not connected");
    return;
  }

  char topic[128];
  char buffer1[512];
  char uid[128];
  JsonDocument doc;

  Serial.println("Publishing HA Discovery...");

  strcpy(topic, "homeassistant/switch/");
  strcat(topic, devUniqueID);
  strcat(topic, "/config");

  strcpy(uid, devUniqueID);
  strcat(uid, "S");

  doc["name"] = "Ugreen USB Hub Switch";
  doc["uniq_id"] = uid;
  doc["stat_t"] = mqtt_state_topic;
  doc["cmd_t"] = mqtt_switch_topic;
  doc["pl_on"] = "Mac";
  doc["pl_off"] = "PC";
  doc["stat_on"] = "Mac";
  doc["stat_off"] = "PC";
  doc["icon"] = "mdi:usb-port";
  doc["optimistic"] = false;
  doc["retain"] = true;
  doc["avty_t"] = mqtt_availability_topic;

  JsonObject device = doc["device"].to<JsonObject>();
  device["name"] = "MQTT USB Hub Switch";
  device["ids"] = "mymqttdevice01";
  device["mf"] = "DIY";
  device["mdl"] = "ESP32-S2";

  serializeJson(doc, buffer1);
  pubsubClient.publish(topic, buffer1, true);

  Serial.println("Discovery published!");
}

void haRemoveDevice() {
  if (!pubsubClient.connected()) {
    Serial.println("Cannot remove device - MQTT not connected");
    return;
  }

  char topic[128];
  Serial.println("Removing device from HA...");
  strcpy(topic, "homeassistant/switch/");
  strcat(topic, devUniqueID);
  strcat(topic, "/config");
  pubsubClient.publish(topic, "");
  Serial.println("Device removed");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT message received on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  if (String(topic) == mqtt_switch_topic) {
    if (message != hubState) {
      switchHub();
    }
  }
}

// Non-blocking MQTT reconnect
bool mqttReconnect() {
  Serial.print("Attempting MQTT connection to ");
  Serial.print(mqtt_broker);
  Serial.print(":");
  Serial.println(mqtt_port);

  String clientId = "USBHubSwitch-" + String(random(0xffff), HEX);

  if (pubsubClient.connect(clientId.c_str(), mqtt_username, mqtt_password,
                           mqtt_availability_topic, 0, true, "offline")) {
    Serial.println("MQTT connected!");
    pubsubClient.subscribe(mqtt_switch_topic);
    pubsubClient.publish(mqtt_state_topic, hubState.c_str(), true);
    pubsubClient.publish(mqtt_availability_topic, "online", true);

    if (auto_discovery) {
      haDiscovery();
    }

    mqttBackoff = MQTT_BACKOFF_MIN;
    return true;
  } else {
    Serial.print("MQTT connection failed, rc=");
    Serial.println(pubsubClient.state());
    mqttBackoff = min(mqttBackoff * 2, MQTT_BACKOFF_MAX);
    return false;
  }
}

void publishState() {
  if (pubsubClient.connected()) {
    pubsubClient.publish(mqtt_state_topic, hubState.c_str(), true);
  }
}

// =============================================================================
// WiFi Manager Setup
// =============================================================================

void setupWiFiManager() {
  // Set hostname before connecting
  WiFi.setHostname("ugreen-usb-hub");

  // Create custom parameters
  custom_mqtt_broker = new WiFiManagerParameter("mqtt_broker", "MQTT Broker", mqtt_broker, 64);
  custom_mqtt_port = new WiFiManagerParameter("mqtt_port", "MQTT Port", mqtt_port, 6);
  custom_mqtt_username = new WiFiManagerParameter("mqtt_user", "MQTT Username", mqtt_username, 32);
  custom_mqtt_password = new WiFiManagerParameter("mqtt_pass", "MQTT Password", mqtt_password, 32);

  // Add parameters to WiFiManager
  wifiManager.addParameter(custom_mqtt_broker);
  wifiManager.addParameter(custom_mqtt_port);
  wifiManager.addParameter(custom_mqtt_username);
  wifiManager.addParameter(custom_mqtt_password);

  // Set callback for saving config
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set config portal timeout (3 minutes)
  wifiManager.setConfigPortalTimeout(180);

  // Set minimum signal quality
  wifiManager.setMinimumSignalQuality(20);

  // Static STA IP; the config portal AP keeps its default 192.168.4.1
  wifiManager.setSTAStaticIPConfig(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS);

  // Try to connect, if it fails start config portal
  Serial.println("Connecting to WiFi...");
  if (!wifiManager.autoConnect("UGreen-USB-Hub-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    Serial.println("Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Read updated parameters
  strlcpy(mqtt_broker, custom_mqtt_broker->getValue(), sizeof(mqtt_broker));
  strlcpy(mqtt_port, custom_mqtt_port->getValue(), sizeof(mqtt_port));
  strlcpy(mqtt_username, custom_mqtt_username->getValue(), sizeof(mqtt_username));
  strlcpy(mqtt_password, custom_mqtt_password->getValue(), sizeof(mqtt_password));

  // Save config if needed
  if (shouldSaveConfig) {
    saveConfig();
  }
}

// =============================================================================
// Status Printing
// =============================================================================

void printWifiStatus() {
  if ((millis() - lastPrintTime) > 30000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected - IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("WiFi disconnected");
    }
    lastPrintTime = millis();
  }
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
  Serial.begin(115200);

  // Diagnostics: boot counter (magic check -> power-on starts at 1) and WiFi event log
  if (bootMagic != BOOT_MAGIC) { bootMagic = BOOT_MAGIC; bootCount = 0; }
  bootCount++;
  WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_LOST_IP);
  delay(1000);
  Serial.println("\n\n=== UGreen USB Hub Switch ===\n");

  // GPIO setup
  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, OUTPUT);
  digitalWrite(switchPin, HIGH);
  pinMode(monitorPin, INPUT_PULLUP);

  // Seed hub state from the pin so a boot on the Mac side is reported correctly
  currentState = digitalRead(monitorPin);
  lastSteadyState = currentState;
  lastFlickerableState = currentState;
  hubState = (currentState == LOW) ? "Mac" : "PC";

  // Load saved configuration
  loadConfig();

  // Setup WiFi with manager
  setupWiFiManager();

  // WiFi robustness: no modem sleep (avoid missed beacons -> reason 200), let the core auto-reconnect.
  // WiFi.persistent() is left as is: WiFiManager already resets it to false after saving credentials.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // Get MAC address and create unique ID
  WiFi.macAddress(macAddr);
  createDiscoveryUniqueID();

  // Initialize hub state
  updateHubState();

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/switch", HTTP_GET, handleSwitch);
  server.on("/state", HTTP_GET, handleState);
  server.on("/discovery_on", HTTP_GET, handleDiscoveryOn);
  server.on("/discovery_off", HTTP_GET, handleDiscoveryOff);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/diag", HTTP_GET, handleDiag);

  // Setup ElegantOTA
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  // Start web server
  server.begin();
  Serial.println("HTTP server started");

  // Setup MQTT
  // Bound blocking while the broker is down: WiFiClient::connect(ip, port) uses _timeout
  // (set here to 1000 ms) for its select(); socket timeout bounds the CONNACK wait.
  espClient.setTimeout(1);
  pubsubClient.setSocketTimeout(1);
  pubsubClient.setServer(mqtt_broker, atoi(mqtt_port));
  pubsubClient.setBufferSize(512);
  pubsubClient.setCallback(mqttCallback);

  Serial.println("\n=== Setup Complete ===\n");
}

// =============================================================================
// Main Loop
// =============================================================================

void loop() {
  // Handle web server requests (always responsive)
  server.handleClient();
  ElegantOTA.loop();

  // WiFi down: let auto-reconnect work; only nudge once per 30 s, 30 s after the last disconnect.
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  if (!wifiUp) {
    unsigned long now = millis();
    if (now - lastDisconnectMs >= WIFI_RECONNECT_GRACE && now - lastManualReconnectMs >= WIFI_RECONNECT_GRACE) {
      Serial.println("WiFi still down, calling WiFi.reconnect()");
      WiFi.reconnect();
      lastManualReconnectMs = now;
    }
  }

  // Non-blocking MQTT reconnect (only while WiFi is up)
  if (!wifiUp) {
    // skip MQTT entirely
  } else if (!pubsubClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt >= mqttBackoff) {
      lastMqttReconnectAttempt = now;
      mqttReconnect();
    }
  } else {
    pubsubClient.loop();
  }

  // Periodic state publish
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatePublish >= STATE_PUBLISH_INTERVAL) {
    publishState();
    lastStatePublish = currentMillis;
  }

  // Update hub state
  updateHubState();

  // Print WiFi status periodically
  printWifiStatus();
}
