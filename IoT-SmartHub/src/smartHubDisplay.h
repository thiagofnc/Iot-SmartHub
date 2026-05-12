#pragma once

#include <Arduino.h>

#include "bluetoothConnection.h"

class SmartHubDisplay {
public:
  void begin();
  void loop();
  void refresh(bool watchConnected, const String &watchName,
               const String &watchModel, int phoneBattery, int ipadBattery,
               const BluetoothConnection::NearbyDevice *nearbyDevices,
               size_t nearbyDeviceCount);
};

extern SmartHubDisplay smartHubDisplay;
