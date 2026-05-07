#include "Lora_Receiver.h"

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

static char rxpacket[BUFFER_SIZE];
static bool lora_idle = true;

// Heltec License Key from your example
static uint32_t license_1[4] = { 0xE45FC246,0x44C995C9,0x3FA18B6F,0xF066DCAF };
static RadioEvents_t RadioEvents;

double Lora_Receiver::lastTemp = 0.0;
double Lora_Receiver::lastHum = 0.0;
bool Lora_Receiver::newDataReceived = false;

void Lora_Receiver::OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    memcpy(rxpacket, payload, size );
    rxpacket[size]='\0';
    Radio.Sleep();
    
    Serial.print("Received: ");
    Serial.print(rxpacket);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.print(" | SNR: ");
    Serial.println(snr);
    
    // Basic parsing assuming format "T:xx.x H:xx.x" or similar
    String packetStr = String(rxpacket);
    int tIndex = packetStr.indexOf("T:");
    int hIndex = packetStr.indexOf("H:");
    if (tIndex >= 0 && hIndex >= 0) {
        lastTemp = packetStr.substring(tIndex + 2, packetStr.indexOf(' ', tIndex)).toDouble();
        lastHum = packetStr.substring(hIndex + 2).toDouble();
        newDataReceived = true;
    }
        
    lora_idle = true; // Always resume receiving
}


Lora_Receiver::Lora_Receiver() {
    freq = 915.0; // Default frequency
}

void Lora_Receiver::init() {
    Serial.println("Lora_Receiver initialized");
    
    // Heltec Initialization
    Mcu.setlicense((unsigned long*)license_1, (unsigned char)HELTEC_BOARD);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    RadioEvents.RxDone = OnRxDone;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                               LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                               LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                               0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
                               
    lora_idle = true;
}

void Lora_Receiver::receivePacket() {
    if(lora_idle) {
        lora_idle = false;
        Radio.Rx(0);
    }
}
