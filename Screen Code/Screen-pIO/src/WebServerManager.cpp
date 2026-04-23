#include "WebServerManager.h"

WebServerManager::WebServerManager() {
    ip = "";
    port = 80;
}

void WebServerManager::init() {
    Serial.println("WebServerManager initialized");
    // TODO: Connect to WiFi and setup WebServer routes
}

void WebServerManager::sendData(String data) {
    // TODO: Send data over HTTP
    Serial.printf("WebServerManager sending data: %s\n", data.c_str());
}

void WebServerManager::getRequest() {
    // TODO: Handle incoming HTTP requests
}
