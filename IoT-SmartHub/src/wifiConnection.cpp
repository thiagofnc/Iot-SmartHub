#include "wifiConnection.h"

WifiConnection wifiConnection;

void WifiConnection::begin(const char *ssid, const char *password) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    setupRoutes();
    server.begin();
    Serial.println("HTTP server started");
  } else {
    Serial.println("WiFi connection failed");
  }
}

void WifiConnection::setupRoutes() {
  server.on("/", HTTP_GET, [this]() {
    String html = "<h1>ESP32 SmartHub</h1>";
    html += "<p>Use /battery?phone=85</p>";
    html += "<p>Phone Battery: ";
    html += phoneBattery;
    html += "%</p>";
    server.send(200, "text/html", html);
  });

  server.on("/battery", HTTP_GET, [this]() {
    if (server.hasArg("phone")) {
      phoneBattery = server.arg("phone").toInt();

      Serial.print("iPhone battery received: ");
      Serial.print(phoneBattery);
      Serial.println("%");

      server.send(200, "text/plain", "Battery received");
    } else {
      server.send(400, "text/plain", "Missing phone value");
    }
  });
}

void WifiConnection::loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}

bool WifiConnection::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String WifiConnection::getIpAddress() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }

  return "No WiFi";
}

int WifiConnection::getPhoneBattery() {
  return phoneBattery;
}