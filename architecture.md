# IoT-SmartHub — System Architecture

The SmartHub is split across four nodes that talk over LoRa (915 MHz),
Wi-Fi/HTTP and UART:

| Node                 | Code path                          | Hardware                |
| -------------------- | ---------------------------------- | ----------------------- |
| Smart Hub (display)  | `Screen Code/Screen-pIO`           | CrowPanel ESP32-S3 7"   |
| Remote LoRa receiver | `Main Code/Main-pIO` (`src/receiver`) | ESP32-S3 DevKitC     |
| Remote LoRa sender   | `Main Code/Main-pIO` (`src/transmitter`) | Heltec WiFi-LoRa 32 V3 |
| Camera (external)    | `Camera Code/Camera-pIO`           | XIAO ESP32-S3 Sense     |

```mermaid
classDiagram
    direction LR

    namespace Smart_Hub_Node {
        class Hub_Main {
            +setup()
            +loop()
        }
        class DisplayManager {
            -CrowPanelDisplay display
            -lv_disp_draw_buf_t drawBuffer
            -ScreenIndex currentScreen
            +initDisplayHardware()
            +initLvgl()
            +showScreen(ScreenIndex s, int dir)
            +swipeScreen(int delta)
            +toggleScreen()
            +updateClockFace()
            +updateGlanceTile()
            +updateWeatherUi()
            +updatePortraitClockUi()
            +tickClock()
            +tickColonBlink()
            +tickMusic()
            +tickSpotify()
            +tickWorldCup()
            +tickWeather()
            +tickLinkState()
            +tickScreenConnectivity()
            +handleSerialCommands()
            +beginScreenConnectivity()
            +applyBleScanResults(BLEScanResults)
            +handleBatteryClient(WiFiClient)
        }
    }

    namespace Remote_Lora_Node {
        class Sender_Main {
            -char txPacket[50]
            -bool loraIdle
            -RadioEvents_t radioEvents
            +setup()
            +loop()
            -onTxDone()
            -onTxTimeout()
        }
        class Env_Sensor {
            -uint8_t pin = 7
            -float temperature
            -float humidity
            +setup(pin, DHT11)
            +getTempAndHumidity() TempAndHumidity
        }
        class Lora_Transmitter {
            -uint32_t freq = 915000000
            -int8_t tx_power = 14
            -uint8_t spreadingFactor = 7
            +Init(RadioEvents_t*)
            +SetChannel(freq)
            +SetTxConfig(...)
            +Send(uint8_t* buf, size_t len)
            +Sleep()
        }
    }

    namespace Remote_Lora_Receiver {
        class Receiver_Main {
            -LoraReceiver lora
            -Motor motor
            -WebServerManager webServer
            -BleCameraReceiver cameraReceiver
            -LocalLightSensor lightSensor
            -bool motorSweepEnabled
            -int currentRotation
            +begin()
            +tick()
            -handleSerial()
            -handleMotorSweep()
        }
        class Lora_Receiver {
            +static double lastTemp
            +static double lastHumidity
            +static bool newDataReceived
            +begin()
            +tick()
            -static onRxDone(payload, size, rssi, snr)
        }
        class Motor {
            -uint8_t motorPin = 41
            -int position
            -Servo servo
            +Motor(uint8_t pin = 41)
            +begin()
            +rotate(int degrees)
        }
        class WebServer {
            +int requestedBrightness = 128
            +int requestedRotation = 0
            +double remoteTemp
            +double remoteHumidity
            +begin()
            +tick()
            -connectWifi()
            -updateOpenWeather()
            -isAuthenticated() bool
            -handleRoot()
            -handleLogin()
            -handleLogout()
            -handleBrightness()
            -handleRotation()
            -handleWeather()
        }
        class BLE_Cam_Receiver {
            +begin()
            +tick()
        }
        class Local_Light_Sensor {
            -uint8_t sensorPin = 5
            -int lightLevel
            +LocalLightSensor(uint8_t pin = 5)
            +begin()
            +tick()
            +level() int
        }
    }

    class Xiao_Camera_Node {
        <<External>>
        -float kHandConfThreshold = 0.32
        -GestureRecognizer g_gesture
        -float* g_eiFeatures
        -RunMode g_mode
        +setup()
        +loop()
        +initCamera() bool
        +ensureFeatureBuffer() bool
        +rgb565ToPackedFloats(fb, n, out)
        +runInferenceCycle()
        +setMode(RunMode)
        +pollSerialControls()
    }

    Hub_Main --> DisplayManager : Renders to
    Hub_Main ..> Receiver_Main : Process Data (Wired)

    Receiver_Main --> Lora_Receiver : Polls data
    Receiver_Main --> Motor : Commands
    Receiver_Main --> WebServer : Hosts
    Receiver_Main --> BLE_Cam_Receiver : Polls gestures
    Receiver_Main --> Local_Light_Sensor : Reads light level

    Sender_Main --> Env_Sensor : Reads
    Sender_Main --> Lora_Transmitter : Broadcasts

    Lora_Transmitter ..> Lora_Receiver : LoRa (Wireless)
    BLE_Cam_Receiver ..> Xiao_Camera_Node : I2C
```

## Notes on the mapping

- **Hub_Main** is the `setup()` / `loop()` in
  [Screen Code/Screen-pIO/src/main.cpp](Screen%20Code/Screen-pIO/src/main.cpp).
- **DisplayManager** is a logical grouping of the LVGL-side free functions
  declared in the same `main.cpp` plus the feature `.inc` files under
  [Screen Code/Screen-pIO/src/features/](Screen%20Code/Screen-pIO/src/features/)
  (`clock_screen`, `weather_screen`, `music_player`, `world_cup_screen`,
  `screen_connectivity`, etc.). Swipes/gestures route through
  `swipeScreen(delta)` and `handleSerialCommands()`.
- **Sender_Main / Env_Sensor / Lora_Transmitter** correspond to the single
  translation unit
  [Main Code/Main-pIO/src/transmitter/main.cpp](Main%20Code/Main-pIO/src/transmitter/main.cpp).
  The `DHTesp dht` instance and the Heltec `Radio` driver are shown as
  separate classes to mirror the UML.
- **Receiver_Main** and the receiver-side classes live under
  [Main Code/Main-pIO/src/receiver/](Main%20Code/Main-pIO/src/receiver/) with
  matching headers in
  [Main Code/Main-pIO/include/receiver/](Main%20Code/Main-pIO/include/receiver/).
  `WebServer` in the diagram is implemented as `WebServerManager` in
  [WebServerManager.h](Main%20Code/Main-pIO/include/web/WebServerManager.h).
- **BLE_Cam_Receiver** is currently a placeholder
  ([BleCameraReceiver.cpp](Main%20Code/Main-pIO/src/receiver/BleCameraReceiver.cpp));
  gesture output from the XIAO camera is emitted as `[GESTURE] X` lines on
  the camera's UART by `runInferenceCycle()` in
  [Camera Code/Camera-pIO/src/main.cpp](Camera%20Code/Camera-pIO/src/main.cpp).
