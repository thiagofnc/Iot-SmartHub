#include <Arduino.h>
#include "LoRaWan_APP.h"
#include <DHT.h>

#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz]
#define LORA_SPREADING_FACTOR                       12        // SF12
#define LORA_CODINGRATE                             3         // [3: 4/7]
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define BUFFER_SIZE                                 50 

// DHT Sensor configuration
#define DHTPIN 4 // Adjust to your actual DHT pin
#define DHTTYPE DHT11 // Change to DHT22 if you are using DHT22
DHT dht(DHTPIN, DHTTYPE);

static char txpacket[BUFFER_SIZE];
static bool lora_idle = true;
uint32_t lastTxTime = 0;
const uint32_t txInterval = 10000; // Transmit every 10 seconds

static uint32_t license_1[4] = { 0xE45FC246,0x44C995C9,0x3FA18B6F,0xF066DCAF };
static RadioEvents_t RadioEvents;

void OnTxDone(void) {
    Serial.println("TX done......");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Serial.println("TX Timeout......");
    lora_idle = true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Transmitter environment starting...");

    dht.begin();

    // Heltec LoRa Initialization
    Mcu.setlicense((unsigned long*)license_1, (unsigned char)HELTEC_BOARD);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
}

void loop() {
    Radio.IrqProcess();

    if (lora_idle && (millis() - lastTxTime >= txInterval)) {
        lastTxTime = millis();
        lora_idle = false;

        float t = dht.readTemperature();
        float h = dht.readHumidity();

        // Check if any reads failed
        if (isnan(t) || isnan(h)) {
            Serial.println("Failed to read from DHT sensor! Using fallback.");
            t = 25.0; // Fallback temp
            h = 50.0; // Fallback hum
        }

        // Format to match Receiver expectaion: "T:xx.x H:xx.x"
        snprintf(txpacket, BUFFER_SIZE, "T:%.1f H:%.1f", t, h);
        
        Serial.printf("Sending: %s\n", txpacket);
        Radio.Send((uint8_t *)txpacket, strlen(txpacket));
    }
}
