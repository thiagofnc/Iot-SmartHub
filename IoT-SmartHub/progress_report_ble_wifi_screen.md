# ESP32 SmartHub Progress Report

## Project Summary

The SmartHub project is being updated from a small OLED-based ESP32 setup to an Elecrow 7-inch ESP32-S3 screen module. The goal is to create a larger dashboard that can show WiFi battery updates, BLE smartwatch status, and nearby BLE device information in one place.

The current design uses three main parts:

- `WifiConnection` for WiFi and Apple Shortcut battery updates.
- `BluetoothConnection` for BLE scanning, smartwatch connection, and nearby BLE device data.
- `SmartHubDisplay` for the Elecrow 7-inch screen layout.

This structure keeps the code cleaner because each class has one main job.

## Why the Design Changed

The original project used a small OLED display. That worked for basic watch status, but it did not have enough space to show more information clearly.

The new Elecrow 7-inch display gives enough room to show:

- Smartwatch connection status
- Smartwatch device name
- Smartwatch model number
- iPhone battery level
- iPad battery level
- Top 5 nearby BLE devices
- RSSI signal value for each nearby BLE device

Because the new screen is larger and uses a different display system, the old OLED code was removed and replaced with LVGL and LovyanGFX.

## WiFi Implementation

The WiFi code is located in:

```text
src/wifiConnection.h
src/wifiConnection.cpp
```

The ESP32 connects to the configured WiFi network:

```cpp
const char *ssid = "iPhone (3)";
const char *password = "Usan_0815";
```

The WiFi code uses a simple Arduino-style web server:

```cpp
WiFiServer server{80};
WiFiClient client = server.available();
```

This approach was chosen because it is simple and similar to the Arduino WiFi web server example. It is easier to understand than adding MQTT or a larger web framework.

The WiFi server receives battery updates from Apple Shortcuts.

iPhone battery URL:

```text
http://ESP32-IP/battery?phone=85
```

iPad battery URL:

```text
http://ESP32-IP/battery?ipad=72
```

The ESP32 stores the values separately and exposes them through:

```cpp
getPhoneBattery()
getIpadBattery()
```

These values are then passed to the display so they can be shown on the screen.

## BLE Implementation

The BLE code is located in:

```text
include/bluetoothConnection.h
src/bluetoothConnection.cpp
```

The BLE code has two main responsibilities.

First, it creates a BLE server named:

```text
IoT-SmartHub
```

This allows another BLE device or app to connect to the SmartHub.

Second, it scans nearby BLE devices. The scan interval is currently set to 3 seconds:

```cpp
bluetoothConfig.scanIntervalMs = 3000;
```

During each scan, the code stores the top 5 nearby BLE devices based on RSSI strength. Each device stores:

```text
name
MAC address
RSSI value
```

If a BLE device has a name, the screen displays the name. If it does not have a name, the screen displays the MAC address.

## Smartwatch Connection

The smartwatch target connection logic is still included. The BLE code scans for the target watch name:

```text
Galaxy Watch5 (5xxh)
```

When the watch is found, the ESP32 saves the MAC address and attempts to connect. If the connection works, the code reads standard BLE device information when available, including:

- Device name
- Model number

These values are displayed on the SmartHub screen.

## Display Implementation

The display code is located in:

```text
src/smartHubDisplay.h
src/smartHubDisplay.cpp
```

The display code was moved into its own class named:

```cpp
SmartHubDisplay
```

This was done to keep `main.cpp` simple. The display class handles:

- Elecrow RGB display setup
- LVGL initialization
- LovyanGFX panel configuration
- Screen layout
- Updating labels
- Showing nearby BLE devices

The main program only needs to call:

```cpp
smartHubDisplay.begin();
smartHubDisplay.loop();
smartHubDisplay.refresh(...);
```

## Screen Layout

The screen is split into two main areas.

Left side:

```text
Watch
Device name
Model
iPhone battery
iPad battery
```

Right side:

```text
Nearby BLE devices
Top 5 devices with RSSI values
```

Example nearby BLE display:

```text
1. Galaxy Watch5
-55 dBm  AA:BB:CC:DD:EE:FF

2. Unknown
-68 dBm  11:22:33:44:55:66
```

This layout was chosen because it makes the dashboard easier to read. The left side shows SmartHub status, and the right side shows live BLE environment information.

## Main Program Flow

The main code is located in:

```text
src/main.cpp
```

Startup flow:

```cpp
Serial.begin(115200);
wifiConnection.begin(ssid, password);
smartHubDisplay.begin();
bluetoothConnection.begin(bluetoothConfig);
```

Loop flow:

```cpp
bluetoothConnection.loop();
wifiConnection.loop();
smartHubDisplay.loop();
refreshDisplay();
```

The display refreshes periodically so that watch status, battery levels, and nearby BLE devices stay updated.

## What Has Been Completed

Completed progress:

- Removed old OLED display code.
- Added Elecrow 7-inch display support.
- Created a separate `SmartHubDisplay` class.
- Added WiFi connection through iPhone hotspot.
- Added iPhone battery reporting through Apple Shortcuts.
- Added iPad battery reporting through Apple Shortcuts.
- Added BLE nearby scanning.
- Added top 5 nearby BLE devices sorted by RSSI.
- Added BLE device name display when available.
- Preserved smartwatch target connection behavior.
- Reduced unnecessary BLE serial printing.
- Updated the dashboard layout for the larger screen.

## Why This Approach Is Useful

This approach is useful because it keeps the system modular.

Each part has a clear responsibility:

```text
WifiConnection
Handles WiFi and HTTP battery updates.

BluetoothConnection
Handles BLE server, smartwatch detection, and nearby BLE scanning.

SmartHubDisplay
Handles the 7-inch display and dashboard layout.

main.cpp
Coordinates all modules.
```

This makes the project easier to debug, easier to explain, and easier to expand later.

## Next Steps

Possible next steps:

- Confirm the Elecrow screen initializes correctly on hardware.
- Confirm iPhone and iPad Shortcuts send battery levels successfully.
- Test BLE scan list on the screen.
- Test smartwatch detection and connection.
- Add cleaner icons or color indicators for battery and BLE strength.
- Add a timestamp for the last battery update.
- Add a fallback screen message when WiFi is not connected.

## Conclusion

The SmartHub has been redesigned into a larger real-time dashboard using WiFi, BLE, and the Elecrow 7-inch display. The system can receive battery levels from Apple Shortcuts, scan nearby BLE devices, connect to the smartwatch target, and display all of this information in a clean two-column layout.

The code is now more organized and easier to maintain because WiFi, BLE, and display logic are separated into their own classes.
