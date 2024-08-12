#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "env.h" // Create this file in your project directory and add your WiFi credentials

// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

IPAddress ip;
String hubState = "PC";
const int switchPin = 35;  // GPIO5 on Wemos S2 Mini
const int monitorPin = 39; // GPIO4 on Wemos S2 Mini for monitoring channel state

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
        }
      }
    }
  }
}

void switchHub() {
  digitalWrite(switchPin, LOW);
  delay(100);
  digitalWrite(switchPin, HIGH);
  delay(500);
  updateHubState();
}

void handleRoot() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family: Arial; text-align: center; color: #ffffff; background:#000000;} #switchButton{background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; text-decoration: none; font-size: 16px; margin: 4px 2px; cursor: pointer;}</style>";
  html += "<script>function updateState() {fetch('/state').then(response => response.text()).then(state => {document.getElementById('state').innerText = state;document.getElementById('switchButton').innerText = 'Switch to ' + (state === 'PC' ? 'Mac' : 'PC');});} setInterval(updateState, 2000);</script>";
  html += "</head><body>";
  html += "<h1>USB Hub Switch</h1>";
  html += "<h2>Current IP: <span id='ipState'>" + ip.toString() + "</span></h2>";
  html += "<p>Current State: <span id='state'>" + hubState + "</span></p>";
  html += String("<a href='/switch' id='switchButton'>Switch to ") + (hubState == "PC" ? "Mac" : "PC") + "</a>";
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

void printWifiStatus() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(ip.toString());
    } else if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
    } else {
        Serial.println("WiFi disconnected");
    }
}

void blinkLED() {
    digitalWrite(15, HIGH);
    delay(1000);
    digitalWrite(15, LOW);
    delay(1000);
}

const int MAX_WIFI_CONNECT_ATTEMPTS = 20;
const int WIFI_RETRY_DELAY = 500; // milliseconds

void setup() {
  Serial.begin(115200);
  pinMode(15, OUTPUT);
  pinMode(switchPin, OUTPUT);
  digitalWrite(switchPin, HIGH);
  pinMode(monitorPin, INPUT_PULLUP);

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
      while(1) { delay(1000); } // Infinite loop to prevent further execution
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  ip = WiFi.localIP();
  Serial.println(ip.toString());

  // Initialize hubState based on current channel
  updateHubState();

  server.on("/", handleRoot);
  server.on("/switch", handleSwitch);
  server.on("/state", handleState);
  server.begin();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.reconnect();
  }
  updateHubState();
  // printWifiStatus();
  // blinkLED();
  server.handleClient();
}