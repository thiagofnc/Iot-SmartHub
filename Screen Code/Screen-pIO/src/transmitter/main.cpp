#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "DHTesp.h"

#define RF_FREQUENCY                                915000000   // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz]
#define LORA_SPREADING_FACTOR                       7         // SF7
#define LORA_CODINGRATE                             3         // [3: 4/7]
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define BUFFER_SIZE                                 50 
#define DHT_PIN                                     7

char txpacket[BUFFER_SIZE];
int seqNum = 1;
bool lora_idle = true;
static RadioEvents_t RadioEvents;
DHTesp dht;

unsigned long license_1[4] = { 0x4ACE2716, 0x75C9401B, 0x0E8AEEB7, 0xB67AD1D2 };


void OnTxDone( void ) {
    lora_idle = true;
}

void OnTxTimeout( void ) {
    Radio.Sleep();
    Serial.println("TX Timeout......");
    lora_idle = true;
}

void setup() {
    Serial.begin(115200);
    
    // Initialize DHT11 sensor
    dht.setup(DHT_PIN, DHTesp::DHT11);
    
    // Heltec LoRa Initialization
    Mcu.setlicense(license_1, HELTEC_BOARD);
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    // V4-specific: Power on the Front-End Module (FEM) and configure TX/RX switch
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);   // FEM enable — powers the antenna path
    pinMode(46, OUTPUT);
    digitalWrite(46, LOW);   // TX/RX switch — LOW for RX default, set HIGH before TX
    
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
    Serial.println("[TX] Starting Heltec Transmitter...");
}

void loop() {
    if(lora_idle == true) {
        delay(10000); // 10-second delay between sends
        
        // Read DHT11 sensor data
        TempAndHumidity newValues = dht.getTempAndHumidity();
        
        // Construct the payload matching the Receiver's parsing format (T:xx.x H:xx.x)
        sprintf(txpacket, "T:%.1f H:%.1f", newValues.temperature, newValues.humidity);
   
        Serial.printf("Sending packet \"%s\" , length %d\r\n", txpacket, strlen(txpacket));
        Radio.Send( (uint8_t *)txpacket, strlen(txpacket) );
        
        seqNum++;
        lora_idle = false;
    }
    Radio.IrqProcess( );
}
