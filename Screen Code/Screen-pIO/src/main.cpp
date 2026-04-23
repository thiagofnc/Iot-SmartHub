#include <Arduino.h>
#include "Receiver_Main.h"

Receiver_Main receiverApp;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- Starting Remote LoRa Receiver ---");
    
    receiverApp.init();
}

void loop() {
    receiverApp.loop();
    delay(10); // Small delay to prevent tight looping / watchdog resets
}
