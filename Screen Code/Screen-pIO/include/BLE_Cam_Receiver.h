#ifndef BLE_CAM_RECEIVER_H
#define BLE_CAM_RECEIVER_H

#include <Arduino.h>

class BLE_Cam_Receiver {
private:
    String img; // Placeholder type for image data

public:
    BLE_Cam_Receiver();
    void init();
    void processGesture();
};

#endif // BLE_CAM_RECEIVER_H
