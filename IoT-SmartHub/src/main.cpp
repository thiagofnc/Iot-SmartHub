#include <Arduino.h>

#include "bluetoothConnection.h"
#include "smartHubDisplay.h"
#include "wifiConnection.h"

namespace {
constexpr unsigned long kStartupDelayMs = 3000;
constexpr unsigned long kDisplayRefreshMs = 2000;
constexpr unsigned long kLoopDelayMs = 200;

const char *ssid = "iPhone (3)";
const char *password = "Usan_0815";

void refreshDisplay() {
  BluetoothConnection::NearbyDevice nearbyDevices[BluetoothConnection::kMaxNearbyDevices];
  const size_t nearbyDeviceCount =
      bluetoothConnection.getNearbyDevices(nearbyDevices,
                                           BluetoothConnection::kMaxNearbyDevices);

  smartHubDisplay.refresh(bluetoothConnection.isTargetConnected(),
                          bluetoothConnection.getTargetDeviceName(),
                          bluetoothConnection.getTargetModelNumber(),
                          wifiConnection.getPhoneBattery(),
                          wifiConnection.getIpadBattery(),
                          nearbyDevices,
                          nearbyDeviceCount);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(kStartupDelayMs);

  Serial.println();
  Serial.println("Booting ESP32 SmartHub...");
  Serial.println("Startup delay complete");

  Serial.println("Starting WiFi...");
  wifiConnection.begin(ssid, password);
  Serial.println("WiFi setup finished");

  Serial.println("Starting display...");
  smartHubDisplay.begin();
  Serial.println("Display setup finished");

  // Refresh the display when BLE status or messages change.
  bluetoothConnection.onConnectionChanged([](bool connected) {
    Serial.println(connected ? "BLE device connected" : "BLE device disconnected");
    refreshDisplay();
  });

  bluetoothConnection.onMessageReceived([](const String &message) {
    Serial.print("BLE received: ");
    Serial.println(message);
    bluetoothConnection.sendMessage("ESP32 got: " + message);
    refreshDisplay();
  });

  BluetoothConnection::Config bluetoothConfig;
  bluetoothConfig.scanIntervalMs = 3000;
  bluetoothConfig.printTargetDetails = false;
  bluetoothConnection.begin(bluetoothConfig);

  Serial.println("BLE SmartHub is advertising");

  refreshDisplay();
}

void loop() {
  bluetoothConnection.loop();
  wifiConnection.loop();
  smartHubDisplay.loop();

  // Refresh periodically so watch info appears after scanning.
  static unsigned long lastDisplayUpdate = 0;
  const unsigned long now = millis();
  if (now - lastDisplayUpdate >= kDisplayRefreshMs) {
    refreshDisplay();
    lastDisplayUpdate = now;
  }

  delay(kLoopDelayMs);
}
