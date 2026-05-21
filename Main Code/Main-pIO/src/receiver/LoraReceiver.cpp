#include "LoraReceiver.h"

namespace {
constexpr uint32_t kRfFrequency = 915000000;
constexpr int8_t kTxOutputPower = 14;
constexpr uint8_t kLoraBandwidth = 0;
constexpr uint8_t kLoraSpreadingFactor = 7;
constexpr uint8_t kLoraCodingRate = 3;
constexpr uint16_t kLoraPreambleLength = 8;
constexpr uint16_t kLoraSymbolTimeout = 0;
constexpr bool kLoraFixedPayload = false;
constexpr bool kLoraIqInversion = false;
constexpr size_t kBufferSize = 50;

char rxPacket[kBufferSize];
bool loraIdle = true;
RadioEvents_t radioEvents;

}  // namespace

double LoraReceiver::lastTemp = 0.0;
double LoraReceiver::lastHumidity = 0.0;
bool LoraReceiver::newDataReceived = false;

void LoraReceiver::begin() {
  Serial.println("LoRa receiver initialized");

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  pinMode(46, OUTPUT);
  digitalWrite(46, LOW);

  radioEvents.RxDone = onRxDone;
  Radio.Init(&radioEvents);
  Radio.SetChannel(kRfFrequency);
  Radio.SetRxConfig(MODEM_LORA, kLoraBandwidth, kLoraSpreadingFactor,
                    kLoraCodingRate, 0, kLoraPreambleLength,
                    kLoraSymbolTimeout, kLoraFixedPayload, 0, true, 0, 0,
                    kLoraIqInversion, true);
}

void LoraReceiver::tick() {
  if (loraIdle) {
    loraIdle = false;
    Radio.Rx(0);
  }
  Radio.IrqProcess();
}

void LoraReceiver::onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  const size_t copySize = min<size_t>(size, kBufferSize - 1);
  memcpy(rxPacket, payload, copySize);
  rxPacket[copySize] = '\0';
  Radio.Sleep();

  Serial.printf("LoRa received: %s | RSSI: %d | SNR: %d\n", rxPacket, rssi, snr);

  String packet(rxPacket);
  const int tIndex = packet.indexOf("T:");
  const int hIndex = packet.indexOf("H:");
  if (tIndex >= 0 && hIndex >= 0) {
    const int tempEnd = packet.indexOf(' ', tIndex);
    lastTemp = packet.substring(tIndex + 2, tempEnd > tIndex ? tempEnd : hIndex).toDouble();
    lastHumidity = packet.substring(hIndex + 2).toDouble();
    newDataReceived = true;
  }

  loraIdle = true;
}
