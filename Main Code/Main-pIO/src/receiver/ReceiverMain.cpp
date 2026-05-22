#include "ReceiverMain.h"

#include <limits.h>

void ReceiverMain::begin() {
  Serial.println("Starting SmartHub main controller");
  lora.begin();
  motor.begin();
  displayLink.begin();
  webServer.begin();
  cameraReceiver.begin(&displayLink);
  lightSensor.begin();
}

void ReceiverMain::tick() {
  if (LoraReceiver::newDataReceived) {
    webServer.remoteTemp = LoraReceiver::lastTemp;
    webServer.remoteHumidity = LoraReceiver::lastHumidity;
    LoraReceiver::newDataReceived = false;
  }

  handleSerial();
  lora.tick();
  cameraReceiver.tick();
  webServer.tick();
  lightSensor.tick();
  pumpBatteryToDisplay();

  if (webServer.requestedRotation != currentRotation) {
    currentRotation = webServer.requestedRotation;
    motor.rotate(currentRotation);
  }

  handleMotorSweep();
}

void ReceiverMain::handleSerial() {
  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command.equalsIgnoreCase("start")) {
    motorSweepEnabled = true;
    Serial.println("Motor sweep started");
  } else if (command.equalsIgnoreCase("stop")) {
    motorSweepEnabled = false;
    Serial.println("Motor sweep stopped");
  }
}

void ReceiverMain::handleMotorSweep() {
  if (!motorSweepEnabled) return;

  const unsigned long now = millis();
  if (now - lastMotorMoveMs < 1000UL) return;
  lastMotorMoveMs = now;

  const int sequence[] = {0, 45, 90, 45};
  motor.rotate(sequence[motorStep]);
  motorStep = (motorStep + 1) % 4;
}

void ReceiverMain::pumpBatteryToDisplay() {
  // Only forward when the value has actually changed — keeps the link quiet
  // and prevents the display from re-flashing "UPDATED FROM SERIAL" on every
  // tick.
  const struct { const char* slot; int value; int* last; } slots[] = {
      {"phone",  webServer.phoneBattery,  &lastSentPhoneBattery},
      {"ipad",   webServer.ipadBattery,   &lastSentIpadBattery},
      {"watch",  webServer.watchBattery,  &lastSentWatchBattery},
      {"device", webServer.deviceBattery, &lastSentDeviceBattery},
  };
  for (const auto& s : slots) {
    if (s.value < 0) continue;
    if (s.value == *s.last) continue;
    *s.last = s.value;
    displayLink.sendBattery(s.slot, s.value);
  }
}
