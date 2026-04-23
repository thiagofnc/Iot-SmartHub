#include "Receiver_Main.h"

Receiver_Main::Receiver_Main() {
    // Constructor
}

void Receiver_Main::init() {
    Serial.println("Receiver_Main initialized");
    
    // Initialize components
    lora.init();
    motor.init();
    webServer.init();
    camReceiver.init();
}

void Receiver_Main::loop() {
    // Poll data and handle core logic
    lora.receivePacket();
    camReceiver.processGesture();
    webServer.getRequest();
    
    // Example motor command logic could go here based on data
}
