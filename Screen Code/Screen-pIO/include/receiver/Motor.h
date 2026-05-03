#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include <ESP32Servo.h>

class Motor {
private:
    int pos;
    Servo servo;
    int motorPin;

public:
    Motor();
    void init();
    void init(int pin);
    void rotate(int deg);
};

#endif // MOTOR_H
