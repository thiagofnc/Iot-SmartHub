#include <Arduino.h>
#include "LoRaWan_APP.h"

#define RF_FREQUENCY                                915000000   // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz]
#define LORA_SPREADING_FACTOR                       7         // SF7
#define LORA_CODINGRATE                             3         // [1: 4/5]
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define BUFFER_SIZE                                 50 

char txpacket[BUFFER_SIZE];
int seqNum = 1;
bool lora_idle = true;
static RadioEvents_t RadioEvents;

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
    // Heltec LoRa Initialization
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
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
        delay(1000); // 1-second delay between sends
        
        // Construct the payload with Sequence Number
        sprintf(txpacket, "[SEQ: %d] Hello", seqNum);
   
        Serial.printf("Sending packet \"%s\" , length %d\r\n", txpacket, strlen(txpacket));
        Radio.Send( (uint8_t *)txpacket, strlen(txpacket) );
        
        seqNum++;
        lora_idle = false;
    }
    Radio.IrqProcess( );
}
