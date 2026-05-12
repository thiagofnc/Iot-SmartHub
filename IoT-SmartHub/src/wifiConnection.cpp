#include "wifiConnection.h"

WifiConnection wifiConnection;

namespace {
constexpr unsigned long kWifiConnectTimeoutMs = 15000;
constexpr unsigned long kWifiRetryDelayMs = 500;
} // namespace

void WifiConnection::begin(const char *ssid, const char *password) {
  // Do not start WiFi when the network name is missing.
  if (ssid == nullptr || ssid[0] == '\0') {
    Serial.println("WiFi SSID is missing");
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  const unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < kWifiConnectTimeoutMs) {
    delay(kWifiRetryDelayMs);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed");
    return;
  }

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  serverStarted = true;
  Serial.println("HTTP server started");
}

void WifiConnection::loop() {
  if (serverStarted && WiFi.status() == WL_CONNECTED) {
    WiFiClient client = server.available();
    if (client) {
      handleClient(client);
    }
  }
}

int WifiConnection::getPhoneBattery() const {
  return phoneBattery;
}

int WifiConnection::getIpadBattery() const {
  return ipadBattery;
}

void WifiConnection::handleClient(WiFiClient &client) {
  // Read one HTTP request, similar to the Arduino WiFi server example.
  Serial.println("New WiFi client");
  client.setTimeout(1000);

  String requestLine = client.readStringUntil('\r');
  client.readStringUntil('\n');

  while (client.connected()) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r" || headerLine.length() == 0) {
      break;
    }
  }

  handleRequestLine(requestLine, client);
  client.stop();
  Serial.println("WiFi client disconnected");
}

void WifiConnection::handleRequestLine(const String &requestLine, WiFiClient &client) {
  // Supported paths: /, /battery?phone=85, and /battery?ipad=72.
  if (updatePhoneBatteryFromRequest(requestLine) ||
      updateIpadBatteryFromRequest(requestLine)) {
    sendTextResponse(client, 200, "OK", "Battery received");
    return;
  }

  if (requestLine.startsWith("GET / ")) {
    sendHomePage(client);
    return;
  }

  sendTextResponse(client, 404, "Not Found", "Route not found");
}

bool WifiConnection::updatePhoneBatteryFromRequest(const String &requestLine) {
  return updateBatteryFromRequest(requestLine, "phone", phoneBattery, "iPhone");
}

bool WifiConnection::updateIpadBatteryFromRequest(const String &requestLine) {
  return updateBatteryFromRequest(requestLine, "ipad", ipadBattery, "iPad");
}

bool WifiConnection::updateBatteryFromRequest(const String &requestLine,
                                              const String &key,
                                              int &batteryLevel,
                                              const char *deviceName) {
  const String prefix = "GET /battery?" + key + "=";
  if (!requestLine.startsWith(prefix)) {
    return false;
  }

  const int valueStart = prefix.length();
  const int valueEnd = requestLine.indexOf(' ', valueStart);
  const String value = valueEnd > valueStart
                           ? requestLine.substring(valueStart, valueEnd)
                           : requestLine.substring(valueStart);
  batteryLevel = constrain(value.toInt(), 0, 100);

  Serial.print(deviceName);
  Serial.print(" battery received: ");
  Serial.print(batteryLevel);
  Serial.println("%");

  return true;
}

void WifiConnection::sendHomePage(WiFiClient &client) {
  // Simple status page for checking the hub from a browser.
  String html = "<h1>ESP32 SmartHub</h1>";
  html += "<p>Use /battery?phone=85</p>";
  html += "<p>Use /battery?ipad=72</p>";
  html += "<p>iPhone Battery: ";
  html += ((phoneBattery >= 0) ? String(phoneBattery) + "%" : "unknown");
  html += "</p>";
  html += "<p>iPad Battery: ";
  html += ((ipadBattery >= 0) ? String(ipadBattery) + "%" : "unknown");
  html += "</p>";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.print(html);
  client.println();
}

void WifiConnection::sendTextResponse(WiFiClient &client, int statusCode,
                                      const char *statusText,
                                      const String &body) {
  client.print("HTTP/1.1 ");
  client.print(statusCode);
  client.print(" ");
  client.println(statusText);
  client.println("Content-type:text/plain");
  client.println("Connection: close");
  client.println();
  client.print(body);
  client.println();
}
