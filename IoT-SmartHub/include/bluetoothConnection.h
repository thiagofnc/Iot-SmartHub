#ifndef BLUETOOTH_CONNECTION_H
#define BLUETOOTH_CONNECTION_H

#include <Arduino.h>
#include <BLEAdvertisedDevice.h>

class BluetoothConnection {
public:
  using MessageHandler = void (*)(const String &message);
  using ConnectionHandler = void (*)(bool connected);
  static constexpr size_t kMaxNearbyDevices = 5;

  struct NearbyDevice {
    String name;
    String address;
    int rssi = -127;
  };

  struct Config {
    const char *deviceName = "IoT-SmartHub";
    const char *serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    const char *rxCharacteristicUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    const char *txCharacteristicUuid = "9b5f54c7-0f72-4f54-9d2f-b15d6fdd8f23";
    const char *targetMacAddress = nullptr;
    unsigned long targetReconnectIntervalMs = 10000;
    bool scanNearbyDevices = true;
    unsigned long scanIntervalMs = 2000;
    uint32_t scanDurationSeconds = 1;
    bool printTargetDetails = false;
  };

  void begin();
  void begin(const Config &config);
  void loop();

  bool isTargetConnected() const;
  String getTargetDeviceName() const;
  String getTargetModelNumber() const;
  size_t getNearbyDevices(NearbyDevice *devices, size_t maxDevices) const;
  void connectToTarget(const char *macAddress);
  void sendMessage(const String &message);

  void onMessageReceived(MessageHandler handler);
  void onConnectionChanged(ConnectionHandler handler);

private:
  class ServerCallbacks;
  class RxCallbacks;
  class ClientCallbacks;

  void handleConnect();
  void handleDisconnect();
  void handleTargetConnect();
  void handleTargetDisconnect();
  void handleMessage(const String &message);
  void attemptTargetConnection();
  void attemptTargetConnection(BLEAdvertisedDevice &device);
  void dumpTargetGatt();
  void readTargetDeviceInfo();
  void scanNearbyDevices();
  void clearNearbyDevices();
  void addNearbyDevice(const String &name, const String &address, int rssi);

  MessageHandler messageHandler = nullptr;
  ConnectionHandler connectionHandler = nullptr;
  bool connected = false;
  bool targetConnected = false;
  bool shouldRestartAdvertising = false;
  bool targetConnectionEnabled = false;
  bool nearbyScanEnabled = false;
  bool targetAutoConnectBlocked = false;
  bool shouldDumpTargetGatt = false;
  bool printTargetDetails = false;
  int targetConnectFailures = 0;
  String targetMacAddress;
  String targetDeviceName = "--";
  String targetModelNumber = "--";
  NearbyDevice nearbyDevices[kMaxNearbyDevices];
  size_t nearbyDeviceCount = 0;
  unsigned long targetReconnectIntervalMs = 10000;
  unsigned long lastTargetConnectAttempt = 0;
  unsigned long scanIntervalMs = 2000;
  uint32_t scanDurationSeconds = 1;
  unsigned long lastScanStarted = 0;
};

extern BluetoothConnection bluetoothConnection;

#endif
