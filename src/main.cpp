#include <Arduino.h>

#include "app/App.h"

homepoint::app::App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
