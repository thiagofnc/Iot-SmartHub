#include "bluetoothConnection.h"

#include <BLE2902.h>
#include <BLEAddress.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>

namespace {
BLEServer *server = nullptr;
BLECharacteristic *txCharacteristic = nullptr;
BLEClient *targetClient = nullptr;
constexpr int kMaxTargetConnectFailures = 3;

bool hasMacAddress(const String &macAddress) {
  return macAddress.length() == 17;
}

bool isGalaxyDeviceName(const std::string &name) {
  String normalized(name.c_str());
  normalized.toLowerCase();
  return normalized == "galaxy watch5 (5xxh)";
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
  nearbyScanEnabled = config.scanNearbyDevices;
  scanIntervalMs = config.scanIntervalMs;
  scanDurationSeconds = config.scanDurationSeconds;

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

  if (!nearbyScanEnabled) {
    attemptTargetConnection();
  }

  if (targetConnected && shouldDumpTargetGatt) {
    shouldDumpTargetGatt = false;
    dumpTargetGatt();
  }

  scanNearbyDevices();
}

bool BluetoothConnection::isConnected() const {
  return connected;
}

bool BluetoothConnection::isTargetConnected() const {
  return targetConnected;
}

String BluetoothConnection::getTargetDeviceName() const {
  return targetDeviceName;
}

String BluetoothConnection::getTargetModelNumber() const {
  return targetModelNumber;
}

void BluetoothConnection::connectToTarget(const char *macAddress) {
  String previousAddress = targetMacAddress;
  targetMacAddress = macAddress == nullptr ? "" : macAddress;
  targetMacAddress.trim();
  targetConnectionEnabled = hasMacAddress(targetMacAddress);
  targetConnected = false;
  lastTargetConnectAttempt = 0;

  if (targetMacAddress != previousAddress) {
    targetConnectFailures = 0;
    targetAutoConnectBlocked = false;
  }

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
  targetConnectFailures = 0;
  shouldDumpTargetGatt = true;
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
    ++targetConnectFailures;
    Serial.println("BLE target connection failed; will retry");
  }
}

void BluetoothConnection::attemptTargetConnection(BLEAdvertisedDevice &device) {
  if (targetConnected) {
    return;
  }

  if (targetConnectFailures >= kMaxTargetConnectFailures) {
    Serial.println("BLE target rejected connection too many times; keeping scan-only mode.");
    targetConnectionEnabled = false;
    targetAutoConnectBlocked = true;
    return;
  }

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

  if (!targetClient->connect(&device)) {
    targetConnected = false;
    ++targetConnectFailures;
    Serial.println("BLE target connection failed; will retry");
  }
}

void BluetoothConnection::dumpTargetGatt() {
  if (targetClient == nullptr || !targetClient->isConnected()) {
    return;
  }

  readTargetDeviceInfo();

  Serial.println();
  Serial.println("Target watch GATT services:");

  std::map<std::string, BLERemoteService *> *services = targetClient->getServices();
  if (services == nullptr || services->empty()) {
    Serial.println("  no services discovered");
    return;
  }

  for (auto &serviceEntry : *services) {
    BLERemoteService *service = serviceEntry.second;
    if (service == nullptr) {
      continue;
    }

    Serial.print("  Service ");
    Serial.println(service->getUUID().toString().c_str());

    std::map<std::string, BLERemoteCharacteristic *> *characteristics =
        service->getCharacteristics();
    if (characteristics == nullptr || characteristics->empty()) {
      Serial.println("    no characteristics");
      continue;
    }

    for (auto &characteristicEntry : *characteristics) {
      BLERemoteCharacteristic *characteristic = characteristicEntry.second;
      if (characteristic == nullptr) {
        continue;
      }

      Serial.print("    Char ");
      Serial.print(characteristic->getUUID().toString().c_str());
      Serial.print(" props=");
      if (characteristic->canRead()) Serial.print("R");
      if (characteristic->canWrite()) Serial.print("W");
      if (characteristic->canWriteNoResponse()) Serial.print("w");
      if (characteristic->canNotify()) Serial.print("N");
      if (characteristic->canIndicate()) Serial.print("I");
      Serial.println();
    }
  }
}

void BluetoothConnection::readTargetDeviceInfo() {
  targetDeviceName = "--";
  targetModelNumber = "--";

  BLERemoteService *genericAccess = targetClient->getService(BLEUUID((uint16_t)0x1800));
  if (genericAccess != nullptr) {
    BLERemoteCharacteristic *deviceName =
        genericAccess->getCharacteristic(BLEUUID((uint16_t)0x2A00));
    if (deviceName != nullptr && deviceName->canRead()) {
      targetDeviceName = String(deviceName->readValue().c_str());
    }
  }

  BLERemoteService *deviceInfo = targetClient->getService(BLEUUID((uint16_t)0x180A));
  if (deviceInfo != nullptr) {
    BLERemoteCharacteristic *modelNumber =
        deviceInfo->getCharacteristic(BLEUUID((uint16_t)0x2A24));
    if (modelNumber != nullptr && modelNumber->canRead()) {
      targetModelNumber = String(modelNumber->readValue().c_str());
    }
  }

  Serial.println();
  Serial.println("Target watch info:");
  Serial.print("  Device name: ");
  Serial.println(targetDeviceName);
  Serial.print("  Model number: ");
  Serial.println(targetModelNumber);
}

void BluetoothConnection::scanNearbyDevices() {
  if (!nearbyScanEnabled || targetConnected) {
    return;
  }

  const unsigned long now = millis();
  if (lastScanStarted != 0 && now - lastScanStarted < scanIntervalMs) {
    return;
  }

  lastScanStarted = now;

  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);

  BLEScanResults results = scan->start(scanDurationSeconds, false);
  const int count = results.getCount();
  bool foundGalaxyDevice = false;

  for (int i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results.getDevice(i);
    const std::string name = device.haveName() ? device.getName() : "";
    if (!isGalaxyDeviceName(name)) {
      continue;
    }

    if (!foundGalaxyDevice) {
      Serial.println();
      Serial.println("Target BLE watch:");
      foundGalaxyDevice = true;
    }

    const std::string serviceUuid =
        device.haveServiceUUID() ? device.getServiceUUID().toString() : "";

    Serial.printf("  %s  RSSI=%d",
                  device.getAddress().toString().c_str(),
                  device.getRSSI());

    Serial.printf("  name=\"%s\"", name.c_str());

    if (!serviceUuid.empty()) {
      Serial.printf("  service=%s", serviceUuid.c_str());
    }

    Serial.println();

    String discoveredAddress(device.getAddress().toString().c_str());
    discoveredAddress.toUpperCase();
    if (targetAutoConnectBlocked && targetMacAddress == discoveredAddress) {
      continue;
    }

    if (!targetConnectionEnabled || targetMacAddress != discoveredAddress) {
      Serial.print("Found target watch; will connect to ");
      Serial.println(discoveredAddress);
      connectToTarget(discoveredAddress.c_str());
    }

    attemptTargetConnection(device);
  }

  scan->clearResults();
}
