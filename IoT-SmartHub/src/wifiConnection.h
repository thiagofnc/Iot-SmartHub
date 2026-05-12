#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiConnection {
public:
  void begin(const char *ssid, const char *password);
  void loop();
  int getPhoneBattery() const;
  int getIpadBattery() const;

private:
  WiFiServer server{80};
  int phoneBattery = -1;
  int ipadBattery = -1;
  bool serverStarted = false;

  void handleClient(WiFiClient &client);
  void handleRequestLine(const String &requestLine, WiFiClient &client);
  bool updatePhoneBatteryFromRequest(const String &requestLine);
  bool updateIpadBatteryFromRequest(const String &requestLine);
  bool updateBatteryFromRequest(const String &requestLine, const String &key,
                                int &batteryLevel, const char *deviceName);
  void sendHomePage(WiFiClient &client);
  void sendTextResponse(WiFiClient &client, int statusCode, const char *statusText,
                        const String &body);
};

extern WifiConnection wifiConnection;
