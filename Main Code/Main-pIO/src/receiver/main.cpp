#include <Arduino.h>

#include "ReceiverMain.h"

ReceiverMain receiver;

void setup() {
  Serial.begin(115200);
  delay(3000);
  receiver.begin();
}

void loop() {
  receiver.tick();
  delay(10);
}
