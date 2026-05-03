#ifndef RECEIVER_MAIN_H
#define RECEIVER_MAIN_H

#include <Arduino.h>
#include "Lora_Receiver.h"
#include "Motor.h"
#include "WebServerManager.h"
#include "BLE_Cam_Receiver.h"
#include "Local_Light_Sensor.h"

class Receiver_Main {
private:
    Lora_Receiver lora;
    Motor motor;
    WebServerManager webServer;
    BLE_Cam_Receiver camReceiver;
    Local_Light_Sensor lightSensor;

    bool motorEnabled;
    unsigned long lastMotorMove;
    int motorStep;

public:
    Receiver_Main();
    void init();
    void loop();
};

#endif // RECEIVER_MAIN_H
