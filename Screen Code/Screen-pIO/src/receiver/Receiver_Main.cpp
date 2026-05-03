#include "Receiver_Main.h"

Receiver_Main::Receiver_Main() {
    motorEnabled = false;
    motorStep = 0;
    lastMotorMove = 0;
}

void Receiver_Main::init() {
    Serial.println("Receiver_Main initialized");
    Serial.println("Type 'start' or 'stop' to control the motor sweep.");
    
    // Initialize components
    lora.init();
    motor.init();
    webServer.init();
    //camReceiver.init();
    //lightSensor.init();
}

void Receiver_Main::loop() {
    // Check for serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim(); // Remove whitespace and newline characters
        if (cmd.equalsIgnoreCase("start")) {
            motorEnabled = true;
            Serial.println("Motor started.");
        } else if (cmd.equalsIgnoreCase("stop")) {
            motorEnabled = false;
            Serial.println("Motor stopped.");
        }
    }

    // Poll data and handle core logic
    //lora.receivePacket();
    //camReceiver.processGesture();
    webServer.getRequest();
    //lightSensor.readLight();
    
    // Motor test sequence (similar to manufacturer example, but non-blocking)
    if (motorEnabled) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastMotorMove >= 1000) {
            lastMotorMove = currentMillis;
            
            if (motorStep == 0) {
                motor.rotate(0);
                motorStep = 1;
            } else if (motorStep == 1) {
                motor.rotate(135);
                motorStep = 2;
            } else if (motorStep == 2) {
                motor.rotate(270);
                motorStep = 3;
            } else if (motorStep == 3) {
                motor.rotate(135);
                motorStep = 0;
            }
        }
    }
}
