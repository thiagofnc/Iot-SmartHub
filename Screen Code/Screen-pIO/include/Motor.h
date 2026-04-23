#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

class Motor {
private:
    int pos;

public:
    Motor();
    void init();
    void rotate(int deg);
};

#endif // MOTOR_H
