#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

class WifiConnection {
public:
  void begin(const char *ssid, const char *password);
  void loop();

  bool isConnected();
  String getIpAddress();

  int getPhoneBattery();

private:
  WebServer server{80};
  int phoneBattery = -1;

  void setupRoutes();
};

extern WifiConnection wifiConnection;
