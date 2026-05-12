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

#include <cctype>
#include <map>

namespace {
BLEServer *server = nullptr;
BLECharacteristic *txCharacteristic = nullptr;
BLEClient *targetClient = nullptr;
constexpr int kMaxTargetConnectFailures = 3;
constexpr unsigned long kAdvertisingRestartDelayMs = 500;

// Accept only the normal BLE MAC format: AA:BB:CC:DD:EE:FF.
bool hasMacAddress(const String &macAddress) {
  if (macAddress.length() != 17) {
    return false;
  }

  for (int i = 0; i < 17; ++i) {
    const char character = macAddress.charAt(i);
    if ((i + 1) % 3 == 0) {
      if (character != ':') {
        return false;
      }
    } else if (!std::isxdigit(static_cast<unsigned char>(character))) {
      return false;
    }
  }

  return true;
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
  // Start BLE as a small UART-style server for phone or app messages.
  BLEDevice::init(config.deviceName);
  targetReconnectIntervalMs = config.targetReconnectIntervalMs;
  nearbyScanEnabled = config.scanNearbyDevices;
  scanIntervalMs = config.scanIntervalMs;
  scanDurationSeconds = config.scanDurationSeconds;
  printTargetDetails = config.printTargetDetails;

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
  // Advertising must be restarted after a client disconnects.
  if (shouldRestartAdvertising) {
    delay(kAdvertisingRestartDelayMs);
    BLEDevice::startAdvertising();
    shouldRestartAdvertising = false;
  }

  if (!nearbyScanEnabled) {
    attemptTargetConnection();
  }

  if (targetConnected && shouldDumpTargetGatt) {
    // Print watch services once after the target connects.
    shouldDumpTargetGatt = false;
    dumpTargetGatt();
  }

  scanNearbyDevices();
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

size_t BluetoothConnection::getNearbyDevices(NearbyDevice *devices,
                                             size_t maxDevices) const {
  if (devices == nullptr || maxDevices == 0) {
    return 0;
  }

  const size_t count = min(nearbyDeviceCount, maxDevices);
  for (size_t i = 0; i < count; ++i) {
    devices[i] = nearbyDevices[i];
  }

  return count;
}

void BluetoothConnection::connectToTarget(const char *macAddress) {
  // Save the target watch address and reset connection retry state.
  String previousAddress = targetMacAddress;
  targetMacAddress = macAddress == nullptr ? "" : macAddress;
  targetMacAddress.trim();
  targetMacAddress.toUpperCase();
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
  // Notify the connected BLE peer when a TX characteristic is available.
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
  shouldDumpTargetGatt = printTargetDetails;
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
  // Retry direct target connections at a fixed interval.
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
  // Connect using the advertised device when the watch is found by scanning.
  if (!targetConnectionEnabled || targetConnected) {
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
  // List discovered services and characteristics for debugging the watch.
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
  // Read standard BLE device information when the target exposes it.
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
  // Scan for nearby BLE devices and connect when the target watch is found.
  if (!nearbyScanEnabled) {
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
  clearNearbyDevices();

  for (int i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results.getDevice(i);
    const std::string name = device.haveName() ? device.getName() : "";
    const String deviceName = name.empty() ? "Unknown" : String(name.c_str());
    String deviceAddress(device.getAddress().toString().c_str());
    deviceAddress.toUpperCase();

    addNearbyDevice(deviceName, deviceAddress, device.getRSSI());

    if (!isGalaxyDeviceName(name)) {
      continue;
    }

    if (printTargetDetails && !foundGalaxyDevice) {
      Serial.println();
      Serial.println("Target BLE watch:");
      foundGalaxyDevice = true;
    }

    if (printTargetDetails) {
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
    }

    if (targetAutoConnectBlocked && targetMacAddress == deviceAddress) {
      continue;
    }

    if (!targetConnectionEnabled || targetMacAddress != deviceAddress) {
      Serial.print("Found target watch; will connect to ");
      Serial.println(deviceAddress);
      connectToTarget(deviceAddress.c_str());
    }

    attemptTargetConnection(device);
  }

  scan->clearResults();
}

void BluetoothConnection::clearNearbyDevices() {
  nearbyDeviceCount = 0;
}

void BluetoothConnection::addNearbyDevice(const String &name,
                                          const String &address,
                                          int rssi) {
  size_t insertIndex = nearbyDeviceCount;
  if (nearbyDeviceCount < kMaxNearbyDevices) {
    ++nearbyDeviceCount;
  } else if (rssi <= nearbyDevices[kMaxNearbyDevices - 1].rssi) {
    return;
  } else {
    insertIndex = kMaxNearbyDevices - 1;
  }

  while (insertIndex > 0 && rssi > nearbyDevices[insertIndex - 1].rssi) {
    nearbyDevices[insertIndex] = nearbyDevices[insertIndex - 1];
    --insertIndex;
  }

  nearbyDevices[insertIndex].name = name;
  nearbyDevices[insertIndex].address = address;
  nearbyDevices[insertIndex].rssi = rssi;
}
