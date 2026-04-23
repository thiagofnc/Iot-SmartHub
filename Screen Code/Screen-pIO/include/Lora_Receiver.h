#ifndef LORA_RECEIVER_H
#define LORA_RECEIVER_H

#include <Arduino.h>

class Lora_Receiver {
private:
    float freq;

public:
    Lora_Receiver();
    void init();
    void receivePacket();
};

#endif // LORA_RECEIVER_H
