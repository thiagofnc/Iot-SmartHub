#include <Arduino.h>
#include <RadioLib.h>

// Set to 1 for Sender, 0 for Receiver
#define IS_SENDER 0

// Heltec WiFi LoRa 32 V3/V4 Pinout for SX1262
#define NSS 8
#define DIO1 14
#define RESET 12
#define BUSY 13

SX1262 radio = new Module(NSS, DIO1, RESET, BUSY);

int txCounter = 0;

// Interrupt flag for the Receiver
volatile bool receivedFlag = false;

// Interrupt Service Routine (ISR)
// MUST be marked IRAM_ATTR to run safely from Flash/RAM in ESP32
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- Heltec V4 RadioLib initialization ---");

  // THE FIX: We MUST pass 1.8V for the TCXO voltage because the V4 board uses it!
  int state = radio.begin(915.0, 125.0, 7, 5, 0x12, 14, 8, 1.8, false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Initialisation successful!");
  } else {
    Serial.printf("Failed, code %d\n", state);
    while (true) {}
  }

  // Bind the DIO1 hardware pin to our interrupt routing
  radio.setDio1Action(setFlag);

  // 1. Force the SX1262 to route signals to the correct antenna port using its internal DIO2 switch
  radio.setDio2AsRfSwitch(true);

  // 1.5 Heltec V4 Exclusives: Turn on the Front-End Module and TX/RX Switch
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH); // V4 FEM EN (powers up the antenna path)
  
  // Hand control of the V4's external TX/RX switch (GPIO 46) directly to RadioLib
  radio.setRfSwitchPins(RADIOLIB_NC, 46); // GPIO 46 is High on TX, Low on RX

  // 2. Disable the internal LDO regulator and use the highly efficient DC-DC converter
  radio.setRegulatorDCDC(); 

  if (IS_SENDER) {
    Serial.println("[Role] SET TO SENDER");
  } else {
    Serial.println("[Role] SET TO RECEIVER");
    
    // Explicitly lock the radio into Continuous Listening Mode!
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("Receiver is now continuously listening via Hardware Interrupt!");
    } else {
      Serial.printf("Failed to start listening, code %d\n", state);
    }
  }
}

void loop() {
  if (IS_SENDER) {
    String str = "Hello from transmitter " + String(txCounter++);
    Serial.printf("Transmitting: %s\n", str.c_str());

    // Blocking transmit
    int state = radio.transmit(str);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(" -> Transmission successful!");
    } else {
      Serial.printf(" -> Transmission failed, code %d\n", state);
    }
    
    // Wait 5 seconds before the next transmission
    delay(5000);

  } else {
    // --- NON-BLOCKING CONTINUOUS RECEIVER LOGIC ---
    // Instead of forcing the radio to constantly reboot its listening window,
    // we simply chill here until the SX1262 physically triggers DIO1 telling the ESP32 a packet landed!
    
    if (receivedFlag) {
      // Temporarily clear the flag
      receivedFlag = false;

      String str;
      
      // Pull the waiting data cleanly out of the sx1262's memory buffer
      int state = radio.readData(str);

      if (state == RADIOLIB_ERR_NONE) {
        Serial.println("\n[Receiver] Packet Received!");
        Serial.printf("  Data: %s\n", str.c_str());
        
        // Get the crucial RSSI and SNR data for your Signal Propagation Proposal
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();
        
        Serial.printf("  RSSI: %.2f dBm\n", rssi);
        Serial.printf("  SNR:  %.2f dB\n", snr);

      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[Receiver] Packet received but corrupted (CRC mismatch)!");
      } else {
        Serial.printf("[Receiver] Receive error, code %d\n", state);
      }
      
      // We must tell the radio to dive back into continuous listening mode!
      radio.startReceive();
    }
  }
}
