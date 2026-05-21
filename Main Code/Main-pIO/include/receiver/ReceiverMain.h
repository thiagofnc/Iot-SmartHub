#pragma once

#include <Arduino.h>

#include "BleCameraReceiver.h"
#include "LocalLightSensor.h"
#include "LoraReceiver.h"
#include "Motor.h"
#include "WebServerManager.h"

class ReceiverMain {
public:
  void begin();
  void tick();

private:
  void handleSerial();
  void handleMotorSweep();

  LoraReceiver lora;
  Motor motor;
  WebServerManager webServer;
  BleCameraReceiver cameraReceiver;
  LocalLightSensor lightSensor;

  bool motorSweepEnabled = false;
  unsigned long lastMotorMoveMs = 0;
  int motorStep = 0;
  int currentRotation = -1;
};
