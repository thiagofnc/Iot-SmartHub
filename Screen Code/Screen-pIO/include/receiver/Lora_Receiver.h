#ifndef LORA_RECEIVER_H
#define LORA_RECEIVER_H

#include <Arduino.h>
#include "LoRaWan_APP.h"

class Lora_Receiver {
private:
    float freq;
    static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

public:
    Lora_Receiver();
    void init();
    void receivePacket();
    
    // Expose latest parsed data
    static double lastTemp;
    static double lastHum;
    static bool newDataReceived;
};

#endif // LORA_RECEIVER_H
