#pragma once

#include <Arduino.h>

#include "BleCameraReceiver.h"
#include "DisplayLink.h"
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
  void pumpBatteryToDisplay();

  LoraReceiver lora;
  Motor motor;
  DisplayLink displayLink;
  WebServerManager webServer;
  BleCameraReceiver cameraReceiver;
  LocalLightSensor lightSensor;

  bool motorSweepEnabled = false;
  unsigned long lastMotorMoveMs = 0;
  int motorStep = 0;
  int currentRotation = -1;

  int lastSentPhoneBattery = INT_MIN;
  int lastSentIpadBattery = INT_MIN;
  int lastSentWatchBattery = INT_MIN;
  int lastSentDeviceBattery = INT_MIN;
};
