#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

class WebServerManager {
public:
  WebServerManager();

  void begin();
  void tick();

  int requestedBrightness = 128;
  int requestedRotation = 0;
  double remoteTemp = 0.0;
  double remoteHumidity = 0.0;

private:
  void connectWifi();
  void updateOpenWeather();
  bool isAuthenticated();

  void handleRoot();
  void handleLogin();
  void handleLogout();
  void handleBrightness();
  void handleRotation();
  void handleWeather();

  WebServer server;
  double currentTemp = 0.0;
  double currentHumidity = 0.0;
  String currentDescription = "No data";
  unsigned long lastWeatherUpdateMs = 0;
};
