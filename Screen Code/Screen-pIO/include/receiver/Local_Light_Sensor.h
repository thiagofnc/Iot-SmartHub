#ifndef LOCAL_LIGHT_SENSOR_H
#define LOCAL_LIGHT_SENSOR_H

#include <Arduino.h>

class Local_Light_Sensor {
private:
    int lightLevel;

public:
    Local_Light_Sensor();
    void init();
    void readLight();
};

#endif // LOCAL_LIGHT_SENSOR_H
