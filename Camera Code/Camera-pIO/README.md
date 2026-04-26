# SmartHub Camera Firmware

Camera firmware source for the Seeed Studio XIAO ESP32S3 Sense camera board.

This folder is part of the single PlatformIO project at the repo root. Use the
root `platformio.ini` environment named `camera`.

## Setup

1. Copy `include/CameraConfig.example.h` to `include/CameraConfig.h`.
2. Fill in your Wi-Fi SSID and password.
3. Open `C:\IoT-SmartHub\Iot-SmartHub` in PlatformIO.
4. Build/upload the `camera` environment.

Command-line upload from the repo root:

```powershell
pio run -e camera -t upload
```

After upload, open the serial monitor at `115200`. The firmware prints the camera URL after it joins Wi-Fi.

## Endpoints

- `/` shows a simple camera page.
- `/capture` returns one JPEG still image.
- `/stream` returns an MJPEG stream.
