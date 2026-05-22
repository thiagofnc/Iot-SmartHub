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

  // Battery levels reported via /battery?<slot>=<percent>. -1 until a first
  // value arrives. Slot names mirror Apple Shortcuts setups: phone, ipad,
  // watch, device.
  int phoneBattery = -1;
  int ipadBattery = -1;
  int watchBattery = -1;
  int deviceBattery = -1;

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
  void handleBattery();

  WebServer server;
  double currentTemp = 0.0;
  double currentHumidity = 0.0;
  String currentDescription = "No data";
  unsigned long lastWeatherUpdateMs = 0;
};
