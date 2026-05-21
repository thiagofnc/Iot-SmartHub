// Screen ESP32 BLE scan + lightweight battery endpoint
// ---------------------------------------------------------------------------
namespace {
constexpr unsigned long SCREEN_BLE_SCAN_INTERVAL_MS = 5000UL;
constexpr uint32_t SCREEN_BLE_SCAN_SECONDS = 1;

BLEScan *screenBleScan = nullptr;
WiFiServer screenBatteryServer(80);
bool screenBatteryServerStarted = false;
unsigned long lastBleScanMs = 0;

uint32_t accentForDeviceName(const String &name) {
  String lower = name;
  lower.toLowerCase();
  if (lower.indexOf("airpods") >= 0 || lower.indexOf("iphone") >= 0 ||
      lower.indexOf("ipad") >= 0) {
    return 0x7CD4F2;
  }
  if (lower.indexOf("pixel") >= 0 || lower.indexOf("nest") >= 0) return 0x38EF7D;
  if (lower.indexOf("galaxy") >= 0 || lower.indexOf("watch") >= 0) return 0xE94E77;
  if (lower.indexOf("sony") >= 0 || lower.indexOf("audio") >= 0) return 0xFF2975;
  return 0x6E7585;
}

const char *iconForDeviceName(const String &name) {
  String lower = name;
  lower.toLowerCase();
  if (lower.indexOf("watch") >= 0) return LV_SYMBOL_LOOP;
  if (lower.indexOf("airpods") >= 0 || lower.indexOf("buds") >= 0 ||
      lower.indexOf("audio") >= 0 || lower.indexOf("head") >= 0) {
    return LV_SYMBOL_AUDIO;
  }
  if (lower.indexOf("camera") >= 0 || lower.indexOf("cam") >= 0) return LV_SYMBOL_EYE_OPEN;
  return LV_SYMBOL_BLUETOOTH;
}

void applyBleScanResults(BLEScanResults &results) {
  const int count = min(results.getCount(), kNearbyMaxDeviceCount);
  nearbyDeviceCount = count;

  for (int i = 0; i < count; ++i) {
    BLEAdvertisedDevice device = results.getDevice(i);
    String name = device.haveName() ? String(device.getName().c_str()) : String("(no name)");
    String address = String(device.getAddress().toString().c_str());

    nearbyDevices[i].name = name;
    nearbyDevices[i].mac = address;
    nearbyDevices[i].rssi = device.getRSSI();
    nearbyDevices[i].vendor = "BLE";
    nearbyDevices[i].companyId = 0x0000;
    nearbyDevices[i].services[0] = device.haveServiceUUID()
                                       ? String(device.getServiceUUID().toString().c_str())
                                       : String("");
    nearbyDevices[i].services[1] = "";
    nearbyDevices[i].services[2] = "";
    nearbyDevices[i].serviceCount = nearbyDevices[i].services[0].isEmpty() ? 0 : 1;
    nearbyDevices[i].txPower = device.haveTXPower() ? device.getTXPower() : INT_MIN;
    nearbyDevices[i].accent = accentForDeviceName(name);
    nearbyDevices[i].icon = iconForDeviceName(name);
  }

  refreshNearbyScreens();
}

bool updateBatteryFromRequest(const String &requestLine, const char *key, int deviceNumber) {
  const String prefix = String("GET /battery?") + key + "=";
  if (!requestLine.startsWith(prefix)) return false;

  const int valueStart = prefix.length();
  const int valueEnd = requestLine.indexOf(' ', valueStart);
  const int value = requestLine.substring(valueStart, valueEnd > valueStart ? valueEnd : requestLine.length()).toInt();
  return setDeviceBattery(deviceNumber, constrain(value, 0, 100));
}

void handleBatteryClient(WiFiClient &client) {
  client.setTimeout(1000);
  String requestLine = client.readStringUntil('\r');
  client.readStringUntil('\n');
  while (client.connected()) {
    String header = client.readStringUntil('\n');
    if (header == "\r" || header.length() == 0) break;
  }

  bool updated = updateBatteryFromRequest(requestLine, "phone", 1) ||
                 updateBatteryFromRequest(requestLine, "ipad", 2) ||
                 updateBatteryFromRequest(requestLine, "watch", 3) ||
                 updateBatteryFromRequest(requestLine, "device", 4);

  if (requestLine.startsWith("GET / ")) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    client.println("<h1>SmartHub Screen</h1><p>Use /battery?phone=85, /battery?ipad=72, /battery?watch=50, or /battery?device=40.</p>");
  } else if (updated) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/plain");
    client.println("Connection: close");
    client.println();
    client.println("OK");
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-type:text/plain");
    client.println("Connection: close");
    client.println();
    client.println("Route not found");
  }
  client.stop();
}
}  // namespace

void beginScreenConnectivity() {
  BLEDevice::init("SmartHub-Screen");
  screenBleScan = BLEDevice::getScan();
  screenBleScan->setActiveScan(true);
  screenBleScan->setInterval(100);
  screenBleScan->setWindow(80);
  Serial.println("Screen BLE scanner ready");
}

void tickScreenConnectivity() {
  if (WiFi.status() == WL_CONNECTED && !screenBatteryServerStarted) {
    screenBatteryServer.begin();
    screenBatteryServerStarted = true;
    Serial.printf("Screen battery endpoint ready at http://%s/\n",
                  WiFi.localIP().toString().c_str());
  }

  if (screenBatteryServerStarted) {
    WiFiClient client = screenBatteryServer.available();
    if (client) handleBatteryClient(client);
  }

  if (!screenBleScan) return;
  const unsigned long now = millis();
  if (now - lastBleScanMs < SCREEN_BLE_SCAN_INTERVAL_MS) return;
  lastBleScanMs = now;

  BLEScanResults results = screenBleScan->start(SCREEN_BLE_SCAN_SECONDS, false);
  applyBleScanResults(results);
  screenBleScan->clearResults();
}
