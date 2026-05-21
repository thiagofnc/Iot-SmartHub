#include "ReceiverMain.h"

void ReceiverMain::begin() {
  Serial.println("Starting SmartHub main controller");
  lora.begin();
  motor.begin();
  webServer.begin();
  cameraReceiver.begin();
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
