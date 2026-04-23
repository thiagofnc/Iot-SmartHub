#include "Local_Light_Sensor.h"

Local_Light_Sensor::Local_Light_Sensor() {
    lightLevel = 0;
}

void Local_Light_Sensor::init() {
    Serial.println("Local_Light_Sensor initialized");
    // TODO: Setup ADC pin for the light sensor
}

void Local_Light_Sensor::readLight() {
    // TODO: Analog read from light sensor pin
    // lightLevel = analogRead(PIN);
}
