#include "Local_Light_Sensor.h"

Local_Light_Sensor::Local_Light_Sensor() {
    lightLevel = 0;
    lastReadTime = 0;
}

void Local_Light_Sensor::init() {
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    Serial.println("Local_Light_Sensor initialized");
}

void Local_Light_Sensor::readLight() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastReadTime >= 60000) { // Poll every 60000ms (1 minute)
        lastReadTime = currentMillis;

        // Read the analog value (typically 0-4095 on ESP32)
        int rawValue = analogRead(LIGHT_SENSOR_PIN);

        Serial.print("Raw light value: ");
        Serial.println(rawValue);

        // Map the 0-4095 value to 0-255 for display brightness
        lightLevel = map(rawValue, 0, 4095, 0, 255);
        
        // Constrain to ensure values stay strictly in the 0-255 bounds
        lightLevel = constrain(lightLevel, 0, 255);
    }
}

int Local_Light_Sensor::getLightLevel() const {
    return lightLevel;
}
