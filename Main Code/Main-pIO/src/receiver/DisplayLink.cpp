#include "DisplayLink.h"

// The display already has a text serial-command dispatcher (handleSerialCommands
// in screen-side serial_commands.inc) that understands "up", "down", "left",
// "right", and "dev<n> <pct>". To avoid duplicating that parser on both sides,
// we emit those exact commands over Serial2 so the display can feed Serial1 into
// the same dispatch path it already uses for the USB serial monitor.

namespace {
constexpr int kDisplayUartRx = 19;
constexpr int kDisplayUartTx = 20;
constexpr uint32_t kDisplayUartBaud = 115200;

// Apple Shortcuts slot name -> battery gauge index on the display (1..4).
// Mirrors the mapping the display's existing /battery HTTP route uses.
int slotToDeviceNumber(const char* slot) {
  if (slot == nullptr) return 0;
  if (strcmp(slot, "phone")  == 0) return 1;
  if (strcmp(slot, "ipad")   == 0) return 2;
  if (strcmp(slot, "watch")  == 0) return 3;
  if (strcmp(slot, "device") == 0) return 4;
  return 0;
}

const char* gestureToCommand(char code) {
  switch (code) {
    case 'U': return "up";
    case 'D': return "down";
    case 'L': return "left";
    case 'R': return "right";
    default:  return nullptr;
  }
}
}  // namespace

void DisplayLink::begin() {
  Serial2.begin(kDisplayUartBaud, SERIAL_8N1, kDisplayUartRx, kDisplayUartTx);
  Serial.printf("Display UART on RX=%d TX=%d @ %u baud\n",
                kDisplayUartRx, kDisplayUartTx,
                static_cast<unsigned>(kDisplayUartBaud));
}

void DisplayLink::sendGesture(char code) {
  const char* cmd = gestureToCommand(code);
  if (cmd == nullptr) return;
  Serial2.print(cmd);
  Serial2.write('\n');
}

void DisplayLink::sendBattery(const char* slot, int percent) {
  const int n = slotToDeviceNumber(slot);
  if (n == 0) return;
  percent = constrain(percent, 0, 100);
  Serial2.printf("dev%d %d\n", n, percent);
}
