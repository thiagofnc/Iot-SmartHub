# IoT-SmartHub Wi-Fi and BLE Connection Presentation

## Slide 1: Title

**IoT-SmartHub: Wi-Fi and BLE Connection**

This part of the project explains how the SmartHub communicates with nearby devices and the local network using Wi-Fi and Bluetooth Low Energy.

Speaker notes:
Today I am presenting the Wi-Fi and BLE connection parts of our IoT-SmartHub. These two connections are important because Wi-Fi lets the ESP32 communicate over the local network, while BLE lets it communicate with nearby devices like a phone or smartwatch.

## Slide 2: Wi-Fi Connection

**What Wi-Fi does in the project**

- The ESP32 connects to the Wi-Fi network as a station.
- The current network name in the code is `RHIT-OPEN`.
- After connecting, the ESP32 prints its IP address to the Serial Monitor.
- It starts a small HTTP web server.
- The web server can receive phone battery data.

Speaker notes:
In the Wi-Fi code, the ESP32 uses `WiFi.mode(WIFI_STA)` to act like a normal Wi-Fi client. Then it calls `WiFi.begin()` with the network name and password. The code waits up to 15 seconds for the connection. If the connection works, it prints the IP address and starts an HTTP server.

## Slide 3: Wi-Fi Web Server

**HTTP routes**

- `/` shows a simple SmartHub web page.
- `/battery?phone=85` receives a phone battery value.
- The battery value is stored inside the SmartHub.
- The server replies with `Battery received` when the value is valid.
- If the value is missing, it returns an error message.

Speaker notes:
The Wi-Fi server gives the SmartHub a simple way to receive data from another device. For example, if a phone sends `/battery?phone=85`, the ESP32 reads the number 85 and stores it as the phone battery percentage. This is a good example of IoT communication because a device sends real status data to the hub over the network.

## Slide 4: BLE Connection

**What BLE does in the project**

- The ESP32 starts BLE using the SmartHub device name.
- It creates a BLE server and service.
- It has an RX characteristic for receiving messages.
- It has a TX characteristic for sending notifications.
- It restarts advertising after a BLE device disconnects.

Speaker notes:
BLE is used for short-range communication. In the code, the SmartHub creates a BLE server with a service and two characteristics. RX means the ESP32 can receive a message from another BLE device. TX means the ESP32 can notify the connected device with a response. This makes the SmartHub work like a small BLE communication hub.

## Slide 5: BLE Watch Scanning

**Galaxy Watch connection**

- The SmartHub scans nearby BLE devices.
- It looks for a device named `Galaxy Watch5 (5XXH)`.
- When found, it tries to connect to that device.
- After connecting, it reads GATT information.
- The OLED shows the watch connection status, device name, and model number.

Speaker notes:
The BLE code also scans for a target watch. It checks nearby BLE device names and looks for a Galaxy Watch. If it finds the watch, it tries to connect. After connection, it reads standard BLE device information, such as the device name and model number. This information is shown on the small OLED screen.

## Slide 6: Main Loop and Display

**How everything works together**

- `bluetoothConnection.loop()` keeps BLE scanning and connections active.
- `wifiConnection.loop()` handles web server requests.
- The PRG button sends a BLE message: `Hello from ESP32 PRG`.
- The OLED updates every 2 seconds.
- The screen shows whether the watch is connected or scanning.

Speaker notes:
In the main program loop, both Wi-Fi and BLE keep running. The Wi-Fi loop handles web requests, and the BLE loop handles advertising, scanning, and target device connection. The PRG button can send a test BLE message. The OLED display keeps the user updated by showing the SmartHub title, watch status, device name, and model number.

## Slide 7: Conclusion

**Why Wi-Fi and BLE are useful**

- Wi-Fi is best for network communication and HTTP data.
- BLE is best for nearby low-power device communication.
- Together, they let the SmartHub receive phone data, talk to BLE devices, and display live connection status.

Speaker notes:
To conclude, Wi-Fi and BLE give the SmartHub two different communication methods. Wi-Fi connects the device to the local network and allows simple web endpoints like the battery route. BLE allows direct nearby communication with devices like a phone or smartwatch. Together, they make the SmartHub more flexible and more useful as an IoT system.

## 3-Minute Timing Guide

- Slide 1: 20 seconds
- Slide 2: 30 seconds
- Slide 3: 30 seconds
- Slide 4: 35 seconds
- Slide 5: 35 seconds
- Slide 6: 30 seconds
- Slide 7: 20 seconds

Total: about 3 minutes
