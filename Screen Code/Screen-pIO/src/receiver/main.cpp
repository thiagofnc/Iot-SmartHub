#include <Arduino.h>
#include "Receiver_Main.h"

Receiver_Main receiverApp;

void setup() {
    Serial.begin(115200);
    // USB CDC (Serial) takes a moment to initialize on the PC side. 
    // Wait longer so you can open the monitor and see the startup messages.
    delay(3000); 
    Serial.println("\n\n--- Starting Remote LoRa Receiver ---");
    
    receiverApp.init();
}

void loop() {
    receiverApp.loop();
    delay(10); // Small delay to prevent tight looping / watchdog resets
}
