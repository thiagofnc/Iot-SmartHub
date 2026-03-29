# IoT-SmartHub

## PlatformIO Setup

1. Install Visual Studio Code and the PlatformIO IDE extension.
2. Copy `esp32-s3-devkitc-1-myboard.json` into PlatformIO's ESP32 boards folder.
   - Windows default location: `C:\Users\<your-user>\.platformio\platforms\espressif32\boards\`
   - If the `espressif32` folder does not exist yet, create any ESP32 PlatformIO project first so PlatformIO downloads that platform.
3. Restart VS Code after copying the board file.
4. In VS Code, open PlatformIO and create a new project.
   - Project name: `IoT-SmartHub`
   - Board: `Espressif ESP32-S3-DevKitC-1-N8 -ELECROW`
   - Framework: `Arduino`
   - If you want the PlatformIO project to live in this folder, uncheck `Use default location` and point the project location here.

![PlatformIO project wizard example](platformio_setup.png)

5. Make sure your `platformio.ini` uses the custom board ID from the JSON filename:

```ini
[env:esp32-s3-devkitc-1-myboard]
platform = espressif32
board = esp32-s3-devkitc-1-myboard
framework = arduino
build_flags =
  -D LV_LVGL_H_INCLUDE_SIMPLE
  -I./include
```

## Reference

For the full Elecrow walkthrough and firmware-side details, use the official guide:

[CrowPanel ESP32 7.0-inch with PlatformIO](https://www.elecrow.com/wiki/CrowPanel_ESP32_7.0-inch_with_PlatformIO.html?srsltid=AfmBOopfj4uwlqOoM2UTBgFlwcAVjQDZ49enjFf_5-aE7s6TExp0BJxs)
