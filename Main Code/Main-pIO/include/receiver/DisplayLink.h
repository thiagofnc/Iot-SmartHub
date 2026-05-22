#pragma once

#include <Arduino.h>

// UART link from the main controller to the CrowPanel display ESP.
// Line-based, ASCII protocol:
//   G <U|D|L|R>\n        gesture forwarded from the camera ESP
//   BAT <slot> <pct>\n   battery update for the named slot (phone/ipad/...)
//
// Wiring (main side): RX=19, TX=20. Crossed to the display side RX=44, TX=43.
class DisplayLink {
public:
  void begin();
  void sendGesture(char code);
  void sendBattery(const char* slot, int percent);
};
