#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class WebServerManager {
private:
    WebServer server;
    String ip;
    int port;
    
    // Weather Data
    double currentTemp;
    double currentHum;
    String currentDesc;
    unsigned long lastWeatherUpdate;
    const unsigned long WEATHER_UPDATE_INTERVAL = 10 * 60 * 1000;
    
    void updateOpenWeatherMap();

public:
    int requestedBrightness;
    int requestedRotation;

    WebServerManager();
    void init();
    void getRequest();
    
    // Web endpoints
    void handleRoot();
    void handleBrightness();
    void handleRotation();
    void handleWeather();
};

#endif // WEBSERVERMANAGER_H
