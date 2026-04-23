#include "BLE_Cam_Receiver.h"

BLE_Cam_Receiver::BLE_Cam_Receiver() {
    img = "";
}

void BLE_Cam_Receiver::init() {
    Serial.println("BLE_Cam_Receiver initialized");
    // TODO: Initialize BLE or I2C communication with Xiao Node
}

void BLE_Cam_Receiver::processGesture() {
    // TODO: Read and process gesture data
}
