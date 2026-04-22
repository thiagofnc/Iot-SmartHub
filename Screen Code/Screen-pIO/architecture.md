# IoT Connected SmartHub Dashboard: Architecture Diagram

Below is the proposed software architecture for the ECE436 Final Project: IoT Connected SmartHub Dashboard.

It outlines the separation of concerns utilizing a Hub-and-Spoke pattern on both the central Smart Hub (which manages the UI, gestures, web server, and servos) and the remote LoRa node (which operates in a low-power mode to read environmental data).

```mermaid
classDiagram
    direction TB
    
    namespace Smart_Hub_Node {
        class Hub_Main {
            +init()
            +loop()
        }
        class DisplayManager {
            +init()
            +drawUI()
            +updateValues()
            +handleSwipes()
        }
        
        class WebServer {
            -ip: string
            -port: int
            +init()
            +sendData(data)
            +getRequest()
        }
        
        class BLE_Cam_Receiver {
            -img: data
            +init()
            +processGesture()
        }
    }

    Hub_Main --> DisplayManager : Renders to
    Hub_Main --> WebServer : Hosts
    Hub_Main --> BLE_Cam_Receiver : Polls gestures



    namespace Remote_Lora_Receiver {


        class Receiver_Main {
            +init()
            +loop()

        }

        class Lora_Receiver {
            -freq: float
            +init()
            +receivePacket()
        }

        class Motor {
            -pos: int
            +init()
            +rotate(deg)
        }


    }

     Receiver_Main --> Lora_Receiver : Polls data
     Receiver_Main --> Motor: Commands



    namespace Remote_Lora_Node {
        class Sender_Main {
            +init()
            +loop()
            +deepSleep()
        }
        class Env_Sensor {
            -temp: float
            -humidity: float
            -light: int
            +init()
            +readData()
        }
        class Lora_Transmitter {
            -freq: float
            -tx_power: int
            +init()
            +sendPacket(p)
        }
    }

    Sender_Main --> Env_Sensor : Reads
    Sender_Main --> Lora_Transmitter : Broadcasts


    %% Wireless Links
    Lora_Receiver <.. Lora_Transmitter : LoRa (Wireless)
    Lora_Receiver <.. Hub_Main : Process Data (Wired)
    class Xiao_Camera_Node {
        <<External>>
    }
    BLE_Cam_Receiver <.. Xiao_Camera_Node : BLE (Wireless)
```
