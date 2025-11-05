#pragma once
#include <Arduino.h>

struct Sample {
  uint32_t timestamp;
  int16_t temperatureCx100;
  uint16_t humidityPctx100;
  uint16_t co2ppm;
  uint16_t pressureDeciHpa; // 0.1 hPa resolution
  uint16_t vocRaw;
  uint16_t smokeRaw;
  uint8_t statusFlags;
};
