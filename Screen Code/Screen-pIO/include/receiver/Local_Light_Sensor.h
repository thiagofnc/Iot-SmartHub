#ifndef LOCAL_LIGHT_SENSOR_H
#define LOCAL_LIGHT_SENSOR_H

#include <Arduino.h>

#define LIGHT_SENSOR_PIN 5 // TODO: Update this to your actual light sensor pin if different

class Local_Light_Sensor {
private:
    int lightLevel;
    unsigned long lastReadTime;

public:
    Local_Light_Sensor();
    void init();
    void readLight();
    int getLightLevel() const;
};

#endif // LOCAL_LIGHT_SENSOR_H
