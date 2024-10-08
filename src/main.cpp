#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "env.h" // Create this file in your project directory and add your WiFi credentials - this is not very secure as it will be included in your build automatically so be careful

// Function declarations
void publishState();
void haDiscovery();

// WIFI IMPORTS
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

IPAddress static_ip(STATIC_IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);

// MQTT IMPORTS
const char* mqtt_broker = MQTT_BROKER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

WiFiClient espClient;
PubSubClient pubsubClient(espClient);

// HUB IMPORTS
String hubState = "PC";
const int switchPin = 35;  // GPIO5 on Wemos S2 Mini
const int monitorPin = 39; // GPIO4 on Wemos S2 Mini for monitoring channel state

//Auto-discover enable/disable option
bool auto_discovery = false;

const unsigned long DEBOUNCE_DELAY = 50; // milliseconds
const unsigned long STATE_CHANGE_THRESHOLD = 1000; // milliseconds
unsigned long lastDebounceTime = 0;
unsigned long lastStateChangeTime = 0;
int lastSteadyState = LOW;
int lastFlickerableState = LOW;
int currentState;

void updateHubState() {
  currentState = digitalRead(monitorPin);
  
  // If the switch changed, due to noise or pressing:
  if (currentState != lastFlickerableState) {
    lastDebounceTime = millis();
    lastFlickerableState = currentState;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    if (currentState != lastSteadyState) {
      lastSteadyState = currentState;
      
      // Check if the state has been steady for the threshold duration
      if ((millis() - lastStateChangeTime) > STATE_CHANGE_THRESHOLD) {
        String newState = (currentState == LOW) ? "PC" : "Mac";
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

void handleRoot() {
  String html = "<html><head>";
  html += "<title>UGreen USB Switch</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family: Arial; text-align: center; color: #ffffff; background:#000000; padding: 5rem;} #switchButton{background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; text-decoration: none; font-size: 16px; margin: 4px 2px; cursor: pointer;} #switchButtonRed{background-color: #FF0000; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; text-decoration: none; font-size: 16px; margin: 4px 2px; cursor: pointer;}</style>";
  html += "<script>function updateState() {fetch('/state').then(response => response.text()).then(state => {document.getElementById('state').innerText = state;document.getElementById('switchButton').innerText = 'Switch to ' + (state === 'PC' ? 'Mac' : 'PC');});} setInterval(updateState, 2000);</script>";
  html += "</head><body>";
  html += "<h1>UGreen USB Switch</h1>";
  html += "<h2>Current IP: <span id='ipState'>" + static_ip.toString() + "</span></h2><br/><br/><br/>";
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


unsigned long lastPrintTime = 0;
void printWifiStatus() {
  // print the ip address ever 30 seconds
  if ((millis() - lastPrintTime) > 30000) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(static_ip.toString());
    } else if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
    } else {
        Serial.println("WiFi disconnected");
    }
    lastPrintTime = millis();
  }

}

void ledOn() {
  digitalWrite(15, HIGH);
}

void ledOff() {
  digitalWrite(15, LOW);
}

void blinkLED(int delayTime = 1000) {
    ledOn();
    delay(delayTime);
    ledOff();
    delay(delayTime);
}

// =============================================================================
// OTA
// =============================================================================
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  blinkLED();
}

unsigned long ota_progress_millis = 0;
void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
    blinkLED();
  } else {
    Serial.println("There was an error during OTA update!");
    blinkLED(500);
  }
}

void elegantOTACallbackInit(void) {
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
}


// =============================================================================
// MQTT
// =============================================================================

byte macAddr[6];                  //Device MAC address
char uidPrefix[] = "rctdev";      //Prefix for unique ID generation (limit to 20 chars)
char devUniqueID[30];             //Generated Unique ID for this device (uidPrefix + last 6 MAC characters) 

const char* mqtt_discovery_prefix = "homeassistant";
const char* mqtt_device_name = "ugreen_usb_hub_switch";
const char* mqtt_switch_topic = "home/usb_hub_switch/switch/set";
const char* mqtt_state_topic = "home/usb_hub_switch/state";
const char* mqtt_availability_topic = "stat/usb_hub_switch/availability";


void createDiscoveryUniqueID() {
  //Generate UniqueID from uidPrefix + last 6 chars of device MAC address
  //This should insure that even multiple devices installed in same HA instance are unique
  
  strcpy(devUniqueID, uidPrefix);
  int preSizeBytes = sizeof(uidPrefix);
  int preSizeElements = (sizeof(uidPrefix) / sizeof(uidPrefix[0]));
  //Now add last 6 chars from MAC address (these are 'reversed' in the array)
  int j = 0;
  for (int i = 2; i >= 0; i--) {
    sprintf(&devUniqueID[(preSizeBytes - 1) + (j)], "%02X", macAddr[i]);   //preSizeBytes indicates num of bytes in prefix - null terminator, plus 2 (j) bytes for each hex segment of MAC
    j = j + 2;
  }
  // End result is a unique ID for this device (e.g. rctdevE350CA) 
  Serial.print("Unique ID: ");
  Serial.println(devUniqueID);  
}

void haDiscovery() {
  char topic[128];
  char buffer1[512];
  char uid[128];
  DynamicJsonDocument doc(1024);
  doc.clear();
  Serial.println("Discovering new devices...");


    Serial.println("Adding Hub switch...");
    //Create unique topic based on devUniqueID
    strcpy(topic, "homeassistant/switch/");
    strcat(topic, devUniqueID);
    strcat(topic, "/config");
    //Create unique_id based on devUniqueID
    strcpy(uid, devUniqueID);
    strcat(uid, "S");
    //Create JSON payload per HA documentation
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
    JsonObject device = doc.createNestedObject("device");
    device["name"] = "MQTT USB Hub Switch";
    device["ids"] = "mymqttdevice01";
    device["mf"] = "DIY";
    device["mdl"] = "ESP32";
    serializeJson(doc, buffer1);   

    //Publish discovery topic and payload (with retained flag)
    pubsubClient.publish(topic, buffer1, true);    
   
    Serial.println("All devices added!");
}

void haRemoveDevice() {
    //Remove all entities by publishing empty payloads
    //Must use original topic, so recreate from original Unique ID
    //This will immediately remove/delete the device/entities from HA
    char topic[128];
    Serial.println("Removing discovered devices...");
    strcpy(topic, "homeassistant/switch/");
    strcat(topic, devUniqueID);
    strcat(topic, "/config");
    pubsubClient.publish(topic, "");

    Serial.println("Devices Removed...");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Switch the hub if the switch topic is received
  if (String(topic) == mqtt_switch_topic) {
    if (message != hubState) {
      switchHub();
    } 
  }
}

void reconnect() {
  while (!pubsubClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "USBHubSwitch-" + String(random(0xffff), HEX);
    if (pubsubClient.connect(clientId.c_str(), mqtt_username, mqtt_password, mqtt_availability_topic, 0, true, "offline")) {
      Serial.println("connected");
      pubsubClient.subscribe(mqtt_switch_topic);
      pubsubClient.publish(mqtt_state_topic, hubState.c_str(), true);
      pubsubClient.publish(mqtt_availability_topic, "online", true);
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

void publishState() {
  pubsubClient.publish(mqtt_state_topic, hubState.c_str(), true);
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
    haDiscovery();
    server.send(200, "text/html", "<h1>Discovery OFF...<h1><h3>Home Assistant MQTT Discovery disabled. Previous entities removed.</h3><a href='/'>Back</a>");
}


// =============================================================================
// Setup
// =============================================================================

const int MAX_WIFI_CONNECT_ATTEMPTS = 20;
const int WIFI_RETRY_DELAY = 2000; // milliseconds

void setup() {
  Serial.begin(115200);
  pinMode(15, OUTPUT);
  pinMode(switchPin, OUTPUT);
  digitalWrite(switchPin, HIGH);
  pinMode(monitorPin, INPUT_PULLUP);

   // Set hostname
  WiFi.setHostname(HOSTNAME);

  // Configure static IP
  if (!WiFi.config(static_ip, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  int wifiConnectAttempts = 0;
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
    wifiConnectAttempts++;

    if (wifiConnectAttempts >= MAX_WIFI_CONNECT_ATTEMPTS) {
      Serial.println("\nFailed to connect to WiFi. Please check your credentials or network status.");
      // You might want to implement a reset or alternative behavior here
      // Reset WiFi and try again
      delay(10000);
      wifiConnectAttempts = 0;
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
    }
  }

  //Get MAC address when joining wifi and place into char array
  WiFi.macAddress(macAddr);
  //Call routing (or embed here) to create initial Unique ID
  createDiscoveryUniqueID();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize hubState based on current channel
  updateHubState();

  // Route for root / web page
  server.on("/", HTTP_GET, handleRoot);

  // Route to switch the hub
  server.on("/switch", HTTP_GET, handleSwitch);
  
  // Route to get the current state
  server.on("/state", HTTP_GET, handleState);

  //Handle web callbacks for enabling or disabling discovery (using this method is just one of many ways to do this)
  server.on("/discovery_on", HTTP_GET, handleDiscoveryOn);

  server.on("/discovery_off", HTTP_GET, handleDiscoveryOff);

  // Start ElegantOTA
  ElegantOTA.begin(&server);    // Start ElegantOTA
  elegantOTACallbackInit();
  server.begin();
  pubsubClient.setServer(mqtt_broker, mqtt_port);
  // Allow for larger payloads on MQTT
  pubsubClient.setBufferSize(512);
  pubsubClient.setCallback(callback);
}

unsigned long lastStatePublish = 0;
const unsigned long STATE_PUBLISH_INTERVAL = 60000;  // 1 minute
void loop() {
  server.handleClient();
  ElegantOTA.loop();  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.reconnect();
  }
  if (!pubsubClient.connected()) {
    reconnect();
  }
  pubsubClient.loop();
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatePublish >= STATE_PUBLISH_INTERVAL) {
    publishState();
    lastStatePublish = currentMillis;
  }
  updateHubState();
  printWifiStatus();
  // blinkLED();
}