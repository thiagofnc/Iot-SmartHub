#pragma once

#include <Arduino.h>

#include "DisplayLink.h"

// Receives gesture codes from the camera ESP over UART (Serial1) and forwards
// them to the display ESP via DisplayLink.
//
// Wiring (main side): RX=44, TX=43. Crossed to the camera side RX=44, TX=43.
// One byte per gesture: 'U'/'D'/'L'/'R'.
class BleCameraReceiver {
public:
  void begin(DisplayLink* display);
  void tick();

private:
  DisplayLink* display_ = nullptr;
};
