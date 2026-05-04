#ifndef BLUETOOTH_CONNECTION_H
#define BLUETOOTH_CONNECTION_H

#include <Arduino.h>

class BluetoothConnection {
public:
  using MessageHandler = void (*)(const String &message);
  using ConnectionHandler = void (*)(bool connected);

  struct Config {
    const char *deviceName = "IoT-SmartHub";
    const char *serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    const char *rxCharacteristicUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    const char *txCharacteristicUuid = "9b5f54c7-0f72-4f54-9d2f-b15d6fdd8f23";
    const char *targetMacAddress = nullptr;
    unsigned long targetReconnectIntervalMs = 10000;
  };

  void begin();
  void begin(const Config &config);
  void loop();

  bool isConnected() const;
  bool isTargetConnected() const;
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

  MessageHandler messageHandler = nullptr;
  ConnectionHandler connectionHandler = nullptr;
  bool connected = false;
  bool targetConnected = false;
  bool shouldRestartAdvertising = false;
  bool targetConnectionEnabled = false;
  String targetMacAddress;
  unsigned long targetReconnectIntervalMs = 10000;
  unsigned long lastTargetConnectAttempt = 0;
};

extern BluetoothConnection bluetoothConnection;

#endif
