#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

class Motor {
public:
  explicit Motor(uint8_t pin = 41);

  void begin();
  void rotate(int degrees);

private:
  uint8_t motorPin;
  int position = 0;
  Servo servo;
};
