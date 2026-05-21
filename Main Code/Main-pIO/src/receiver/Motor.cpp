#include "Motor.h"

Motor::Motor(uint8_t pin) : motorPin(pin) {}

void Motor::begin() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servo.setPeriodHertz(50);
  servo.attach(motorPin, 500, 2500);
  rotate(0);
  Serial.printf("Motor initialized on pin %u\n", motorPin);
}

void Motor::rotate(int degrees) {
  degrees = constrain(degrees, 0, 90);
  if (degrees == position) return;

  const int step = degrees > position ? 1 : -1;
  for (int current = position; current != degrees; current += step) {
    servo.writeMicroseconds(map(current, 0, 90, 500, 1200));
    delay(10);
  }

  servo.writeMicroseconds(map(degrees, 0, 90, 500, 1150));
  position = degrees;
  Serial.printf("Motor rotated to %d degrees\n", position);
}
