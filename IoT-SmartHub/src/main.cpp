#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "bluetoothConnection.h"
#include "wifiConnection.h"

constexpr int screenWidth = 128;
constexpr int screenHeight = 64;
constexpr uint8_t oledAddress = 0x3C;

constexpr uint8_t prgButtonPin = 0;
constexpr unsigned long debounceMs = 50;

const char *ssid = "RHIT-OPEN";
const char *password = "";
const char *samsungWatchMacAddress = "24:11:53:C0:DF:EE";

Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, RST_OLED);

String lastAction = "Waiting for BLE text";
bool bleConnected = false;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void enableBoardPower() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void drawWrappedText(int startY, const String &text) {
  const int maxCharsPerLine = 21;
  int y = startY;
  int index = 0;
  const int textLength = static_cast<int>(text.length());

  while (index < textLength && y <= 54) {
    int end = min(index + maxCharsPerLine, textLength);

    if (end < textLength) {
      int breakAt = text.lastIndexOf(' ', end - 1);
      if (breakAt >= index) {
        end = breakAt + 1;
      }
    }

    String line = text.substring(index, end);
    line.trim();

    if (line.length() == 0 && end < textLength) {
      index = end;
      continue;
    }

    display.setCursor(0, y);
    display.print(line);
    y += 10;
    index = end;
  }
}

void refreshDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("ESP32 SmartHub");

  display.setCursor(0, 12);
  display.print(bleConnected ? "BLE: Connected" : "BLE: Advertising");

  display.setCursor(0, 24);
  display.print("WiFi: ");
  display.print(wifiConnection.isConnected() ? wifiConnection.getIpAddress() : "No WiFi");

  display.setCursor(0, 36);
  display.print("iPhone Batt: ");
  int phoneBattery = wifiConnection.getPhoneBattery();

  if (phoneBattery >= 0) {
    display.print(phoneBattery);
    display.print("%");
  } else {
    display.print("--%");
  }

  drawWrappedText(46, lastAction);

  display.display();
}

void updateButton() {
  bool reading = digitalRead(prgButtonPin);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceMs && reading != stableButtonState) {
    stableButtonState = reading;

    if (stableButtonState == LOW) {
      const String outgoingMessage = "Hello from ESP32 PRG";
      bluetoothConnection.sendMessage(outgoingMessage);
      lastAction = "TX: " + outgoingMessage;
      Serial.println(lastAction);
      refreshDisplay();
    }
  }

  lastButtonReading = reading;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Booting ESP32 SmartHub...");

  pinMode(prgButtonPin, INPUT_PULLUP);

  enableBoardPower();
  delay(100);

  Wire.begin(SDA_OLED, SCL_OLED);

  if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddress, true, false)) {
    Serial.println("OLED init failed");
  }

  display.clearDisplay();
  display.display();

  bluetoothConnection.onConnectionChanged([](bool connected) {
    bleConnected = connected;
    lastAction = connected ? "Connected to phone" : "Waiting for BLE text";
    Serial.println(connected ? "BLE device connected" : "BLE device disconnected");
    refreshDisplay();
  });

  bluetoothConnection.onMessageReceived([](const String &message) {
    lastAction = "RX: " + message;
    Serial.print("BLE received: ");
    Serial.println(message);
    bluetoothConnection.sendMessage("ESP32 got: " + message);
    refreshDisplay();
  });

  BluetoothConnection::Config bluetoothConfig;
  bluetoothConfig.targetMacAddress = samsungWatchMacAddress;
  bluetoothConnection.begin(bluetoothConfig);
  wifiConnection.begin(ssid, password);
  Serial.println("BLE SmartHub is advertising");

  refreshDisplay();
}

void loop() {
  bluetoothConnection.loop();
  wifiConnection.loop();
  updateButton();

  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 2000) {
    refreshDisplay();
    lastDisplayUpdate = millis();
  }
}
