#include <Arduino.h>
#include "Receiver_Main.h"

// football-data.org API key (free tier: 10 requests/min)
const char* WC_API_KEY = "d0421e6457b34162a0c59a71f9384405";

Receiver_Main receiverApp(WC_API_KEY);

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
