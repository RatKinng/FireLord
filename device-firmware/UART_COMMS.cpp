#include "UART_COMMS.h"

// For testing only; we'll use a central main.cpp

UART_COMMS uart(Serial1);  // D6 TX, D7 RX on XIAO SAMD21

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);           // Many LoRa-E5 boards default to 9600
  delay(100);
  if (LoRaE5::ping(uart)) Serial.println("LoRa-E5 OK");
  String ver;
  if (LoRaE5::version(uart, ver)) Serial.println("VER:\n" + ver);
  // Example: set test mode then send a small hex packet
  LoRaE5::setMode(uart, 2);      // 2 = LoRa P2P test
  LoRaE5::p2pConfig(uart, 915000000, 7, 125, 8, 14);
  LoRaE5::p2pTxHex(uart, "48656C6C6F");  // "Hello"
}

void loop() {}
