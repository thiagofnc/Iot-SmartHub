#include "Motor.h"

Motor::Motor() {
    pos = 0;
}

void Motor::init() {
    Serial.println("Motor initialized");
    // TODO: Setup PWM or ESP32Servo here
}

void Motor::rotate(int deg) {
    pos = deg;
    Serial.printf("Motor rotating to %d degrees\n", deg);
    // TODO: Actuate motor
}
