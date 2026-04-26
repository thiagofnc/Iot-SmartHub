# IoT-SmartHub

## PlatformIO Setup

This repo is one PlatformIO project with separate firmware folders:

- `Screen Code/Screen-pIO` - current CrowPanel screen firmware.
- `Camera Code/Camera-pIO` - Seeed Studio XIAO ESP32S3 Sense camera firmware.

Open `C:\IoT-SmartHub\Iot-SmartHub` in VS Code. The root `platformio.ini`
exposes two upload environments:

- `screen` uploads the CrowPanel screen firmware.
- `camera` uploads the XIAO ESP32S3 Sense camera firmware.

Do not open the nested screen or camera folders as separate PlatformIO projects.
They are only source folders now.

1. Install Visual Studio Code and the PlatformIO IDE extension.
2. Copy `esp32-s3-devkitc-1-myboard.json` into PlatformIO's ESP32 boards folder.
   - Windows default location: `C:\Users\<your-user>\.platformio\platforms\espressif32\boards\`
   - If the `espressif32` folder does not exist yet, create any ESP32 PlatformIO project first so PlatformIO downloads that platform.
3. Restart VS Code after copying the board file.
4. Open `C:\IoT-SmartHub\Iot-SmartHub` in VS Code.
5. In PlatformIO, use Project Tasks to upload either environment:
   - `screen > General > Upload`
   - `camera > General > Upload`

![PlatformIO project wizard example](platformio_setup.png)

The screen environment uses the custom board ID from the JSON filename:

```ini
[env:screen]
platform = espressif32
board = esp32-s3-devkitc-1-myboard
framework = arduino
```

Create your local app settings file before building the screen firmware.
   - Copy `Screen Code/Screen-pIO/include/AppConfig.example.h` to `Screen Code/Screen-pIO/include/AppConfig.h`
   - Fill in your own Wi-Fi name, password, OpenWeather API key, location, units, and Spotify values
   - `AppConfig.h` is gitignored, so your private values stay off GitHub

## Camera Firmware Setup

The camera firmware targets the Seeed Studio XIAO ESP32S3 Sense with PlatformIO's
`seeed_xiao_esp32s3` board ID. It uses the Sense camera expansion board pin map
for the OV2640 module.

1. Copy `Camera Code/Camera-pIO/include/CameraConfig.example.h` to `Camera Code/Camera-pIO/include/CameraConfig.h`.
2. Fill in your Wi-Fi SSID and password.
3. Open the repo root, `C:\IoT-SmartHub\Iot-SmartHub`, in PlatformIO.
4. Build/upload the `camera` environment.

Command-line upload:

```powershell
pio run -e camera -t upload
```

After upload, open the serial monitor at `115200`. The firmware prints the local
camera URL when Wi-Fi connects. Use `/capture` for a still JPEG or `/stream` for
MJPEG streaming.

## Spotify Setup

The music screen uses Spotify's Web API to read the currently playing track,
show album art, and send play/pause/next/previous commands to your active
Spotify device. It does not stream Spotify audio on the ESP32.

In `Screen Code/Screen-pIO/include/AppConfig.h`, set:

```cpp
constexpr char SPOTIFY_CLIENT_ID[] = "your client id";
constexpr char SPOTIFY_CLIENT_SECRET[] = "your client secret";
constexpr char SPOTIFY_REFRESH_TOKEN[] = "your refresh token";
constexpr char SPOTIFY_MARKET[] = "US";
constexpr char SPOTIFY_DEVICE_ID[] = "";
```

Generate the refresh token with these scopes:

```text
user-read-currently-playing user-read-playback-state user-modify-playback-state
```

Leave `SPOTIFY_DEVICE_ID` empty to control the currently active Spotify
Connect device. Spotify playback-control endpoints require a Spotify Premium
account and an active player.

## Repo Notes

- `Screen Code/Screen-pIO/include/AppConfig.h` is intentionally local-only.
- `Camera Code/Camera-pIO/include/CameraConfig.h` is intentionally local-only.
- The only PlatformIO project config is `platformio.ini` at the repo root.
- Anyone cloning the repo still needs the custom board definition file installed in their local PlatformIO boards folder.

## Reference

For the full Elecrow walkthrough and firmware-side details, use the official guide:

[CrowPanel ESP32 7.0-inch with PlatformIO](https://www.elecrow.com/wiki/CrowPanel_ESP32_7.0-inch_with_PlatformIO.html?srsltid=AfmBOopfj4uwlqOoM2UTBgFlwcAVjQDZ49enjFf_5-aE7s6TExp0BJxs)
