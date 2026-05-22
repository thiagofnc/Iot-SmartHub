#include "BleCameraReceiver.h"

namespace {
constexpr int kCameraUartRx = 44;
constexpr int kCameraUartTx = 43;
constexpr uint32_t kCameraUartBaud = 115200;

bool isGestureChar(int c) {
  return c == 'U' || c == 'D' || c == 'L' || c == 'R';
}
}  // namespace

void BleCameraReceiver::begin(DisplayLink* display) {
  display_ = display;
  Serial1.begin(kCameraUartBaud, SERIAL_8N1, kCameraUartRx, kCameraUartTx);
  Serial.printf("Camera UART on RX=%d TX=%d @ %u baud\n",
                kCameraUartRx, kCameraUartTx,
                static_cast<unsigned>(kCameraUartBaud));
}

void BleCameraReceiver::tick() {
  while (Serial1.available()) {
    const int c = Serial1.read();
    if (!isGestureChar(c)) continue;
    Serial.printf("[CAM] gesture %c\n", static_cast<char>(c));
    if (display_ != nullptr) display_->sendGesture(static_cast<char>(c));
  }
}
