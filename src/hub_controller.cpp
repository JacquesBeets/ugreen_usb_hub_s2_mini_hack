#include "hub_controller.h"
#include "mqtt_manager.h"

String hubState = "PC";
unsigned long lastDebounceTime = 0;
unsigned long lastStateChangeTime = 0;
int lastSteadyState = LOW;
int lastFlickerableState = LOW;
int currentState;

void setupHubController() {
  updateHubState();
}

void updateHubState() {
  currentState = digitalRead(MONITOR_PIN);
  
  if (currentState != lastFlickerableState) {
    lastDebounceTime = millis();
    lastFlickerableState = currentState;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentState != lastSteadyState) {
      lastSteadyState = currentState;
      
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
  Serial.println("Current State: " + hubState);
  digitalWrite(SWITCH_PIN, LOW);
  delay(100);
  digitalWrite(SWITCH_PIN, HIGH);
  delay(500);
  updateHubState();
}