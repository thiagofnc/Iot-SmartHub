#pragma once

#include <Arduino.h>
#include <LoRaWan_APP.h>

class LoraReceiver {
public:
  void begin();
  void tick();

  static double lastTemp;
  static double lastHumidity;
  static bool newDataReceived;

private:
  static void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
};
