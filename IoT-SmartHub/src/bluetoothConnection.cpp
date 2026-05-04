#include "bluetoothConnection.h"

#include <BLE2902.h>
#include <BLEAddress.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {
BLEServer *server = nullptr;
BLECharacteristic *txCharacteristic = nullptr;
BLEClient *targetClient = nullptr;

bool hasMacAddress(const String &macAddress) {
  return macAddress.length() == 17;
}
} // namespace

class BluetoothConnection::ServerCallbacks : public BLEServerCallbacks {
public:
  explicit ServerCallbacks(BluetoothConnection &connection)
      : connection(connection) {}

  void onConnect(BLEServer *) override {
    connection.handleConnect();
  }

  void onDisconnect(BLEServer *) override {
    connection.handleDisconnect();
  }

private:
  BluetoothConnection &connection;
};

class BluetoothConnection::RxCallbacks : public BLECharacteristicCallbacks {
public:
  explicit RxCallbacks(BluetoothConnection &connection)
      : connection(connection) {}

  void onWrite(BLECharacteristic *characteristic) override {
    std::string rawValue = characteristic->getValue();
    String value(rawValue.c_str());

    if (value.length() > 0) {
      connection.handleMessage(value);
    }
  }

private:
  BluetoothConnection &connection;
};

class BluetoothConnection::ClientCallbacks : public BLEClientCallbacks {
public:
  explicit ClientCallbacks(BluetoothConnection &connection)
      : connection(connection) {}

  void onConnect(BLEClient *) override {
    connection.handleTargetConnect();
  }

  void onDisconnect(BLEClient *) override {
    connection.handleTargetDisconnect();
  }

private:
  BluetoothConnection &connection;
};

BluetoothConnection bluetoothConnection;

void BluetoothConnection::begin() {
  Config config;
  begin(config);
}

void BluetoothConnection::begin(const Config &config) {
  BLEDevice::init(config.deviceName);
  targetReconnectIntervalMs = config.targetReconnectIntervalMs;

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(*this));

  BLEService *service = server->createService(config.serviceUuid);

  BLECharacteristic *rxCharacteristic = service->createCharacteristic(
      config.rxCharacteristicUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxCharacteristic->setCallbacks(new RxCallbacks(*this));

  txCharacteristic = service->createCharacteristic(
      config.txCharacteristicUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(config.serviceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  if (config.targetMacAddress != nullptr) {
    connectToTarget(config.targetMacAddress);
  }
}

void BluetoothConnection::loop() {
  if (shouldRestartAdvertising) {
    delay(500);
    BLEDevice::startAdvertising();
    shouldRestartAdvertising = false;
  }

  attemptTargetConnection();
}

bool BluetoothConnection::isConnected() const {
  return connected;
}

bool BluetoothConnection::isTargetConnected() const {
  return targetConnected;
}

void BluetoothConnection::connectToTarget(const char *macAddress) {
  targetMacAddress = macAddress == nullptr ? "" : macAddress;
  targetMacAddress.trim();
  targetConnectionEnabled = hasMacAddress(targetMacAddress);
  targetConnected = false;
  lastTargetConnectAttempt = 0;

  if (!targetConnectionEnabled) {
    Serial.println("BLE target MAC is invalid; expected format AA:BB:CC:DD:EE:FF");
  }
}

void BluetoothConnection::sendMessage(const String &message) {
  if (!connected || txCharacteristic == nullptr) {
    return;
  }

  txCharacteristic->setValue(message.c_str());
  txCharacteristic->notify();
}

void BluetoothConnection::onMessageReceived(MessageHandler handler) {
  messageHandler = handler;
}

void BluetoothConnection::onConnectionChanged(ConnectionHandler handler) {
  connectionHandler = handler;
}

void BluetoothConnection::handleConnect() {
  connected = true;

  if (connectionHandler != nullptr) {
    connectionHandler(true);
  }
}

void BluetoothConnection::handleDisconnect() {
  connected = false;
  shouldRestartAdvertising = true;

  if (connectionHandler != nullptr) {
    connectionHandler(false);
  }
}

void BluetoothConnection::handleTargetConnect() {
  targetConnected = true;
  Serial.print("Connected to BLE target: ");
  Serial.println(targetMacAddress);
}

void BluetoothConnection::handleTargetDisconnect() {
  targetConnected = false;
  Serial.print("Disconnected from BLE target: ");
  Serial.println(targetMacAddress);
}

void BluetoothConnection::handleMessage(const String &message) {
  if (messageHandler != nullptr) {
    messageHandler(message);
  }
}

void BluetoothConnection::attemptTargetConnection() {
  if (!targetConnectionEnabled || targetConnected) {
    return;
  }

  const unsigned long now = millis();
  if (lastTargetConnectAttempt != 0 &&
      now - lastTargetConnectAttempt < targetReconnectIntervalMs) {
    return;
  }

  lastTargetConnectAttempt = now;

  if (targetClient == nullptr) {
    targetClient = BLEDevice::createClient();
    targetClient->setClientCallbacks(new ClientCallbacks(*this));
  }

  if (targetClient->isConnected()) {
    targetConnected = true;
    return;
  }

  Serial.print("Connecting to BLE target: ");
  Serial.println(targetMacAddress);

  BLEAddress address(targetMacAddress.c_str());
  if (!targetClient->connect(address)) {
    targetConnected = false;
    Serial.println("BLE target connection failed; will retry");
  }
}
