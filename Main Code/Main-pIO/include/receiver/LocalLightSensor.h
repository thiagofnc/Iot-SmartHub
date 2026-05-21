#pragma once

#include <Arduino.h>

class LocalLightSensor {
public:
  explicit LocalLightSensor(uint8_t pin = 5);

  void begin();
  void tick();
  int level() const;

private:
  uint8_t sensorPin;
  int lightLevel = 0;
  unsigned long lastReadMs = 0;
};
