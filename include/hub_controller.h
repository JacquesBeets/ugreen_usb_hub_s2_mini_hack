#ifndef HUB_CONTROLLER_H
#define HUB_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

void setupHubController();
void updateHubState();
void switchHub();

extern String hubState;

#endif // HUB_CONTROLLER_H