#include "LocalLightSensor.h"

LocalLightSensor::LocalLightSensor(uint8_t pin) : sensorPin(pin) {}

void LocalLightSensor::begin() {
  pinMode(sensorPin, INPUT);
  Serial.println("Local light sensor initialized");
}

void LocalLightSensor::tick() {
  const unsigned long now = millis();
  if (now - lastReadMs < 60000UL) return;
  lastReadMs = now;

  const int rawValue = analogRead(sensorPin);
  lightLevel = constrain(map(rawValue, 0, 4095, 0, 255), 0, 255);
  Serial.printf("Light raw=%d level=%d\n", rawValue, lightLevel);
}

int LocalLightSensor::level() const {
  return lightLevel;
}
