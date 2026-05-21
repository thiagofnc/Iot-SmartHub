#include <Arduino.h>
#include <DHTesp.h>
#include <LoRaWan_APP.h>

namespace {
constexpr uint32_t kRfFrequency = 915000000;
constexpr int8_t kTxOutputPower = 14;
constexpr uint8_t kLoraBandwidth = 0;
constexpr uint8_t kLoraSpreadingFactor = 7;
constexpr uint8_t kLoraCodingRate = 3;
constexpr uint16_t kLoraPreambleLength = 8;
constexpr bool kLoraFixedPayload = false;
constexpr bool kLoraIqInversion = false;
constexpr uint8_t kDhtPin = 7;
constexpr size_t kBufferSize = 50;

char txPacket[kBufferSize];
bool loraIdle = true;
RadioEvents_t radioEvents;
DHTesp dht;

void onTxDone() {
  loraIdle = true;
}

void onTxTimeout() {
  Radio.Sleep();
  Serial.println("LoRa TX timeout");
  loraIdle = true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  dht.setup(kDhtPin, DHTesp::DHT11);

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  pinMode(46, OUTPUT);
  digitalWrite(46, LOW);

  radioEvents.TxDone = onTxDone;
  radioEvents.TxTimeout = onTxTimeout;
  Radio.Init(&radioEvents);
  Radio.SetChannel(kRfFrequency);
  Radio.SetTxConfig(MODEM_LORA, kTxOutputPower, 0, kLoraBandwidth,
                    kLoraSpreadingFactor, kLoraCodingRate, kLoraPreambleLength,
                    kLoraFixedPayload, true, 0, 0, kLoraIqInversion, 3000);
  Serial.println("LoRa transmitter started");
}

void loop() {
  if (loraIdle) {
    delay(10000);
    TempAndHumidity values = dht.getTempAndHumidity();
    snprintf(txPacket, sizeof(txPacket), "T:%.1f H:%.1f",
             values.temperature, values.humidity);
    Serial.printf("Sending %s\n", txPacket);
    Radio.Send(reinterpret_cast<uint8_t *>(txPacket), strlen(txPacket));
    loraIdle = false;
  }
  Radio.IrqProcess();
}
